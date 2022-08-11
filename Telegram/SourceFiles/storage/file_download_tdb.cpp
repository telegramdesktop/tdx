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
#include "tdb/tdb_tl_scheme.h"
#include "tdb/tdb_file_proxy.h"

namespace {

constexpr auto kMaxReadPart = int64(1024 * 1024);

using namespace Tdb;

} // namespace

TdbFileLoader::TdbFileLoader(
	not_null<Main::Session*> session,
	FileId fileId,
	LocationType type,
	const QString &toFile,
	int64 loadSize,
	int64 fullSize,
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
	} else if (!_requestId && !_loadingLifetime) {
		sendRequest();
	}
}

void TdbFileLoader::sendRequest() {
	cancelRequest();
	_requestId = session().sender().request(TLdownloadFile(
		tl_int32(_fileId),
		tl_int32(1),
		tl_int53(_loadOffset),
		tl_int53(_loadSize - _loadOffset),
		tl_bool(false)
	)).done([=](const TLfile &result) {
		_requestId = 0;
		const auto weak = base::make_weak(this);
		apply(result, weak);
		if (weak && !_requestId && !_finished) {
			session().tdb().updates(
			) | rpl::start_with_next([=](const TLupdate &update) {
				update.match([&](const TLDupdateFile &data) {
					apply(data.vfile(), weak);
				}, [](const auto &) {});
			}, _loadingLifetime);
		}
	}).fail([=](const Error &error) {
		_requestId = 0;
		cancel(true);
	}).send();
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

void TdbFileLoader::apply(
		const TLfile &file,
		const base::weak_ptr<TdbFileLoader> &weak) {
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
	if (!active && (!_loadSize || readyForRead < required)) {
		cancel(true);
		return;
	} else if (readyForRead <= 0) {
		Assert(active);
		return;
	} else if (!active) {
		cancelRequest();
	}
	if (!_proxy) {
		_proxy = FileProxy::Create(local.vpath().v);
		if (!_proxy) {
			// Maybe the file was moved and we wait for a new updateFile.
			return;
		}
	}
	auto leftToRead = readyForRead;
	if (!_proxy->seek(_loadOffset)) {
		cancel(true);
		return;
	}
	while (leftToRead > 0) {
		const auto read = std::min(leftToRead, kMaxReadPart);
		const auto bytes = _proxy->read(read);
		if (bytes.size() != read) {
			cancel(true);
			return;
		}
		if (!feedPart(_loadOffset, bytes) || !weak) {
			break;
		}
		_loadOffset += read;
		leftToRead -= read;
	}
}

bool TdbFileLoader::setFinalSize(int64 size) {
	if (!_fullSize) {
		Assert(_loadSize == 0);
		_fullSize = _loadSize = size;
		return true;
	} else if (_fullSize == size) {
		return true;
	}
	LOG(("Tdb Error: "
		"Bad size provided by TDLib for file %1: %2, real: %3, loading: %4"
		).arg(_fileId
		).arg(_fullSize
		).arg(size
		).arg(_loadSize));
	cancel(true);
	return false;
}

bool TdbFileLoader::feedPart(int64 offset, const QByteArray &bytes) {
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

int64 TdbFileLoader::currentOffset() const {
	return _loadOffset;
}

Storage::Cache::Key TdbFileLoader::cacheKey() const {
	return {}; // todo file.id is not persistent across relaunches.
}

std::optional<MediaKey> TdbFileLoader::fileLocationKey() const {
	return {}; // todo file.id is not persistent across relaunches.
}

void TdbFileLoader::cancelRequest() {
	if (_requestId) {
		session().sender().request(base::take(_requestId)).cancel();
	}
	_loadingLifetime.destroy();
}

void TdbFileLoader::increaseLoadSizeHook() {
	sendRequest();
}

void TdbFileLoader::cancelHook() {
	_proxy = nullptr;
	if (_requestId || _loadingLifetime) {
		cancelRequest();
		session().sender().request(TLcancelDownloadFile(
			tl_int32(_fileId),
			tl_bool(false)
		)).send();
	}
}
