/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_media_preload.h"

#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "media/streaming/media_streaming_reader.h"
#include "storage/file_download.h" // kMaxFileInMemory.

#include "tdb/tdb_account.h"
#include "tdb/tdb_tl_scheme.h"

namespace Data {
namespace {

using namespace Tdb;

constexpr auto kDefaultPreloadPrefix = 4 * 1024 * 1024;

[[nodiscard]] int64 ChoosePreloadPrefix(not_null<DocumentData*> video) {
	const auto result = video->videoPreloadPrefix();
	return result
		? result
		: std::min(int64(kDefaultPreloadPrefix), video->size);
}

} // namespace

MediaPreload::MediaPreload(Fn<void()> done)
: _done(std::move(done)) {
}

void MediaPreload::callDone() {
	if (const auto onstack = _done) {
		onstack();
	}
}

PhotoPreload::PhotoPreload(
	not_null<PhotoData*> photo,
	FileOrigin origin,
	Fn<void()> done)
: MediaPreload(std::move(done))
, _photo(photo->createMediaView()) {
	start(origin);
}

PhotoPreload::~PhotoPreload() {
	if (_photo) {
		base::take(_photo)->owner()->cancel();
	}
}

bool PhotoPreload::Should(
		not_null<PhotoData*> photo,
		not_null<PeerData*> context) {
	return !photo->cancelled()
		&& AutoDownload::Should(
			photo->session().settings().autoDownload(),
			context,
			photo);
}

void PhotoPreload::start(FileOrigin origin) {
	if (_photo->loaded()) {
		callDone();
	} else {
		_photo->owner()->load(origin, LoadFromCloudOrLocal, true);
		_photo->owner()->session().downloaderTaskFinished(
		) | rpl::filter([=] {
			return _photo->loaded();
		}) | rpl::start_with_next([=] { callDone(); }, _lifetime);
	}
}

#if 0 // mtp
VideoPreload::VideoPreload(
	not_null<DocumentData*> video,
	FileOrigin origin,
	Fn<void()> done)
: MediaPreload(std::move(done))
, DownloadMtprotoTask(
	&video->session().downloader(),
	video->videoPreloadLocation(),
	origin)
, _video(video)
, _full(video->size) {
	if (Can(video)) {
		check();
	} else {
		callDone();
	}
}

void VideoPreload::check() {
	const auto key = _video->bigFileBaseCacheKey();
	const auto weak = base::make_weak(static_cast<has_weak_ptr*>(this));
	_video->owner().cacheBigFile().get(key, [weak](
			const QByteArray &result) {
		if (!result.isEmpty()) {
			crl::on_main([weak] {
				if (const auto strong = weak.get()) {
					static_cast<VideoPreload*>(strong)->callDone();
				}
			});
		} else {
			crl::on_main([weak] {
				if (const auto strong = weak.get()) {
					static_cast<VideoPreload*>(strong)->load();
				}
			});
		}
	});
}

void VideoPreload::load() {
	if (!Can(_video)) {
		callDone();
		return;
	}
	const auto prefix = ChoosePreloadPrefix(_video);
	Assert(prefix > 0 && prefix <= _video->size);
	const auto part = Storage::kDownloadPartSize;
	const auto parts = (prefix + part - 1) / part;
	for (auto i = 0; i != parts; ++i) {
		_parts.emplace(i * part, QByteArray());
	}
	addToQueue();
}

void VideoPreload::done(QByteArray result) {
	const auto key = _video->bigFileBaseCacheKey();
	if (!result.isEmpty() && key) {
		Assert(result.size() < Storage::kMaxFileInMemory);
		_video->owner().cacheBigFile().putIfEmpty(
			key,
			Storage::Cache::Database::TaggedValue(std::move(result), 0));
	}
	callDone();
}

VideoPreload::~VideoPreload() {
	if (!_finished && !_failed) {
		cancelAllRequests();
	}
}

bool VideoPreload::Can(not_null<DocumentData*> video) {
	return video->canBeStreamed(nullptr)
		&& video->videoPreloadLocation().valid()
		&& video->bigFileBaseCacheKey();
}

bool VideoPreload::readyToRequest() const {
	const auto part = Storage::kDownloadPartSize;
	return !_failed && (_nextRequestOffset < _parts.size() * part);
}

int64 VideoPreload::takeNextRequestOffset() {
	Expects(readyToRequest());

	_requestedOffsets.emplace(_nextRequestOffset);
	_nextRequestOffset += Storage::kDownloadPartSize;
	return _requestedOffsets.back();
}

bool VideoPreload::feedPart(
		int64 offset,
		const QByteArray &bytes) {
	Expects(offset < _parts.size() * Storage::kDownloadPartSize);
	Expects(_requestedOffsets.contains(int(offset)));
	Expects(bytes.size() <= Storage::kDownloadPartSize);

	const auto part = Storage::kDownloadPartSize;
	_requestedOffsets.remove(int(offset));
	_parts[offset] = bytes;
	if ((_nextRequestOffset + part >= _parts.size() * part)
		&& _requestedOffsets.empty()) {
		_finished = true;
		removeFromQueue();
		auto result = ::Media::Streaming::SerializeComplexPartsMap(_parts);
		if (result.size() == _full) {
			// Make sure it is parsed as a complex map.
			result.push_back(char(0));
		}
		done(std::move(result));
	}
	return true;
}

void VideoPreload::cancelOnFail() {
	_failed = true;
	cancelAllRequests();
	done({});
}

bool VideoPreload::setWebFileSizeHook(int64 size) {
	_failed = true;
	cancelAllRequests();
	done({});
	return false;
}
#endif

[[nodiscard]] QByteArray PackPreload(const QByteArray &bytes, int64 full) {
	if (bytes.isEmpty()) {
		return {};
	}

	auto parts = base::flat_map<uint32, QByteArray>();
	const auto part = Storage::kDownloadPartSize;
	const auto size = int(bytes.size());
	const auto count = (size + part - 1) / part;
	parts.reserve(count);
	const auto begin = bytes.constData();
	const auto end = begin + size;
	for (auto data = begin; data < end; data += part) {
		const auto offset = int(data - begin);
		const auto length = std::min(part, size - offset);
		parts.emplace(offset, QByteArray::fromRawData(data, length));
	}
	auto result = ::Media::Streaming::SerializeComplexPartsMap(parts);
	if (result.size() == full) {
		// Make sure it is parsed as a complex map.
		result.push_back(char(0));
	}
	return result;
}

VideoPreload::VideoPreload(
	not_null<DocumentData*> video,
	Fn<void()> done)
: MediaPreload(std::move(done))
, _video(video)
, _session(&video->session())
, _sender(&_session->sender())
, _full(video->size)
, _fileId(video->tdbFileId())
, _prefix(ChoosePreloadPrefix(video)) {
	Expects(_prefix > 0 && _prefix <= _full);

	if (Can(video)) {
		check();
	} else {
		callDone();
	}
}

bool VideoPreload::Can(not_null<DocumentData*> video) {
	return video->canBeStreamed(nullptr)
		&& (video->tdbFileId() != 0)
		&& video->bigFileBaseCacheKey();
}

void VideoPreload::check() {
	const auto key = _video->bigFileBaseCacheKey();
	const auto weak = base::make_weak(static_cast<has_weak_ptr*>(this));
	_video->owner().cacheBigFile().get(key, [weak](
			const QByteArray &result) {
		if (!result.isEmpty()) {
			crl::on_main([weak] {
				if (const auto strong = weak.get()) {
					static_cast<VideoPreload*>(strong)->callDone();
				}
			});
		} else {
			crl::on_main([weak] {
				if (const auto strong = weak.get()) {
					static_cast<VideoPreload*>(strong)->load();
				}
			});
		}
	});
}

void VideoPreload::load() {
	if (!Can(_video)) {
		callDone();
		return;
	}
	_sender.request(TLgetFile(
		tl_int32(_fileId)
	)).done([=](const TLDfile &data) {
		startWith(data.vlocal().data());
	}).fail([=] {
		done({});
	}).send();
}

void VideoPreload::done(QByteArray result) {
	const auto key = _video->bigFileBaseCacheKey();
	if (!result.isEmpty() && key) {
		Assert(result.size() < Storage::kMaxFileInMemory);
		_video->owner().cacheBigFile().putIfEmpty(
			key,
			Storage::Cache::Database::TaggedValue(std::move(result), 0));
	}
	callDone();
}

void VideoPreload::startWith(const TLDlocalFile &data) {
	if (data.vdownloaded_prefix_size().v > 0
		|| data.vis_downloading_active().v) {
		finishWith(data);
	} else {
		_sender.request(TLdownloadFile(
			tl_int32(_fileId),
			tl_int32(kDefaultDownloadPriority),
			tl_int53(0), // offset
			tl_int53(_prefix),
			tl_bool(false) // synchronous
		)).done([=](const TLDfile &data) {
			if (continueWith(data.vlocal().data())) {
				_downloadLifetime = _session->tdb().updates(
				) | rpl::start_with_next([=](const Tdb::TLupdate &update) {
					if (update.type() == Tdb::id_updateFile) {
						const auto &file = update.c_updateFile().vfile();
						if (file.data().vid().v == _fileId) {
							continueWith(file.data().vlocal().data());
						}
					}
				});
			}
		}).fail([=] {
			done({});
		}).send();
	}
}

bool VideoPreload::continueWith(const TLDlocalFile &data) {
	if (!data.vis_downloading_active().v
		|| data.vdownload_offset().v > 0
		|| data.vdownloaded_prefix_size().v >= _prefix) {
		finishWith(data);
		return false;
	}
	return true;
}

void VideoPreload::finishWith(const TLDlocalFile &data) {
	_downloadLifetime.destroy();
	const auto loaded = !data.vdownload_offset().v
		&& (data.vdownloaded_prefix_size().v >= _prefix);
	if (loaded) {
		_sender.request(TLreadFilePart(
			tl_int32(_fileId),
			tl_int53(0), // offset
			tl_int53(_prefix)
		)).done([=](const TLDfilePart &data) {
			done(PackPreload(data.vdata().v, _full));
		}).fail([=] {
			done({});
		}).send();
	} else {
		done({});
	}
}

VideoPreload::~VideoPreload() {
}

} // namespace Data
