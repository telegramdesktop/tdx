/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "storage/file_download_tdb.h"

#include "storage/cache/storage_cache_types.h"
#include "main/main_session.h"
#include "data/data_document.h"
#include "tdb/tdb_sender.h"
#include "tdb/tdb_account.h"

namespace {

constexpr auto kMaxReadPart = 1024 * 1024;

using namespace Tdb;

} // namespace

TdbFileLoader::TdbFileLoader(
	not_null<Main::Session*> session,
	FileId fileId,
	LocationType type,
	const QString &toFile,
	int loadSize,
	int fullSize,
	LoadToCacheSetting toCache,
	LoadFromCloudSetting fromCloud,
	bool autoLoading,
	uint8 cacheTag)
: FileLoader(
	session,
	toFile,
	loadSize,
	fullSize,
	type,
	toCache,
	fromCloud,
	autoLoading,
	cacheTag)
, _fileId(fileId) {
}

TdbFileLoader::~TdbFileLoader() {
	if (!_finished) {
		cancel();
	}
}

FileId TdbFileLoader::fileId() const {
	return _fileId;
}

void TdbFileLoader::startLoading() {
	if (_finished) {
		return;
	} else if (!_loading) {
		_loading = true;

		session().sender().request(TLdownloadFile(
			tl_int32(_fileId),
			tl_int32(1),
			tl_int32(_loadOffset),
			tl_int32(_loadSize - _loadOffset),
			tl_bool(false)
		)).done([=](const TLfile &result) {
			apply(result);
		}).fail([=](const Error &error) {
			cancel(true);
		}).send();

		session().tdb().updates(
		) | rpl::start_with_next([=](const TLupdate &update) {
			update.match([&](const TLDupdateFile &data) {
				apply(data.vfile());
			}, [](const auto &) {});
		}, _lifetime);
	}
}

void TdbFileLoader::startLoadingWithPartial(const QByteArray &data) {
	Expects(data.startsWith("partial:"));

	constexpr auto kPrefix = 8;
	const auto use = data.size() - kPrefix;
	if (use > 0) {
		_loadOffset = use;
		feedPart(0, QByteArray::fromRawData(data.data() + kPrefix, use));
	}
	startLoading();
}

void TdbFileLoader::apply(const TLfile &file) {
	const auto &fields = file.data();
	if (fields.vid().v != _fileId) {
		return;
	}
	if (!setFinalSize(fields.vsize().v)) {
		return;
	}
	const auto &local = fields.vlocal().data();
	const auto available = local.vdownload_offset().v
		+ local.vdownloaded_prefix_size().v
		- _loadOffset;
	const auto required = _loadSize - _loadOffset;
	const auto readyForRead = std::min(required, available);
	const auto active = local.vis_downloading_active().v;
	if (!active) {
		_loading = false;
	}
	if (readyForRead < required && !_loading) {
		cancel(true);
		return;
	} else if (readyForRead <= 0) {
		return;
	}
	if (!_reading.isOpen()) {
		_reading.setFileName(local.vpath().v);
		if (!_reading.open(QIODevice::ReadOnly)) {
			cancel(true);
			return;
		}
	}
	auto leftToRead = readyForRead;
	_reading.seek(_loadOffset);
	while (leftToRead > 0) {
		const auto read = std::min(leftToRead, kMaxReadPart);
		const auto bytes = _reading.read(read);
		if (bytes.size() != read) {
			cancel(true);
			return;
		}
		if (!feedPart(_loadOffset, bytes)) {
			break;
		}
		_loadOffset += read;
		leftToRead -= read;
	}
}

bool TdbFileLoader::setFinalSize(int size) {
	if (!_fullSize || _fullSize == size) {
		_fullSize = _loadSize = size;
		return true;
	}
	LOG(("Tdb Error: "
		"Bad size provided by TDLib for file %1: %2, real: %3"
		).arg(_fileId
		).arg(_fullSize
		).arg(size));
	cancel(true);
	return false;
}

bool TdbFileLoader::feedPart(int offset, const QByteArray &bytes) {
	const auto buffer = bytes::make_span(bytes);
	if (!writeResultPart(offset, buffer)) {
		return false;
	}
	const auto loadedTill = _loadOffset + bytes.size();
	const auto finished = (_fullSize && loadedTill >= _loadSize);
	if (finished) {
		if (!finalizeResult()) {
			return false;
		}
	} else {
		notifyAboutProgress();
	}
	return true;
}

int TdbFileLoader::currentOffset() const {
	return _ready;
}

Storage::Cache::Key TdbFileLoader::cacheKey() const {
	return Data::TdbFileCacheKey(_fileId);
}

std::optional<MediaKey> TdbFileLoader::fileLocationKey() const {
	if (_locationType != UnknownFileLocation) {
		return mediaKey(_locationType, 0, uint32(_fileId));
	}
	return std::nullopt;
}

void TdbFileLoader::cancelHook() {
	_reading.close();
	if (_loading) {
		session().sender().request(TLcancelDownloadFile(
			tl_int32(_fileId),
			tl_bool(false)
		)).send();
		_loading = false;
	}
	_lifetime.destroy();
}
