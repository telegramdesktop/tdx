/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "media/streaming/media_streaming_loader_tdb.h"

#include "storage/streamed_file_downloader.h"
#include "storage/cache/storage_cache_types.h"
#include "tdb/tdb_account.h"
#include "tdb/tdb_file_proxy.h"
#include "tdb/tdb_tl_scheme.h"

namespace Media {
namespace Streaming {
namespace {

// If not-loaded part is further from loaded part than this offset.
constexpr auto kResendRequestOffset = int64(2 * 128 * 1024);

constexpr auto kLoadLimit = int64(8 * 128 * 1024);

using namespace Tdb;

} // namespace

LoaderTdb::LoaderTdb(
	not_null<Account*> account,
	FileId fileId,
	Storage::Cache::Key baseCacheKey,
	int64 size)
: _account(account)
, _fileId(fileId)
, _baseCacheKey(baseCacheKey)
, _size(size) {
}

LoaderTdb::~LoaderTdb() {
	removeFromQueue();
}

Storage::Cache::Key LoaderTdb::baseCacheKey() const {
	return _baseCacheKey;
}

int64 LoaderTdb::size() const {
	return _size;
}

void LoaderTdb::load(int64 offset) {
	crl::on_main(this, [=] {
		if (_downloader) {
			auto bytes = _downloader->readLoadedPart(offset);
			if (!bytes.isEmpty()) {
				cancelForOffset(offset);
				_parts.fire({ offset, std::move(bytes) });
				return;
			}
		}
		if (haveSentRequestForOffset(offset)) {
			return;
		} else if (_requested.add(offset)) {
			addToQueueWithPriority();
		}
	});
}

void LoaderTdb::addToQueueWithPriority() {
	addToQueue(_priority);
}

void LoaderTdb::stop() {
	crl::on_main(this, [=] {
		_requested.clear();
		removeFromQueue();
	});
}

void LoaderTdb::tryRemoveFromQueue() {
	crl::on_main(this, [=] {
		if (_requested.empty() && !haveSentRequests()) {
			removeFromQueue();
		}
	});
}

void LoaderTdb::cancel(int64 offset) {
	crl::on_main(this, [=] {
		cancelForOffset(offset);
	});
}

void LoaderTdb::cancelForOffset(int64 offset) {
	if (haveSentRequestForOffset(offset)) {
		cancelRequestForOffset(offset);
	} else {
		_requested.remove(offset);
	}
}

void LoaderTdb::attachDownloader(
		not_null<Storage::StreamedFileDownloader*> downloader) {
	_downloader = downloader;
}

void LoaderTdb::clearAttachedDownloader() {
	_downloader = nullptr;
}

void LoaderTdb::resetPriorities() {
	crl::on_main(this, [=] {
		_requested.resetPriorities();
	});
}

void LoaderTdb::setPriority(int priority) {
	if (_priority == priority) {
		return;
	}
	_priority = priority;
	if (haveSentRequests()) {
		addToQueueWithPriority();
	}
}

void LoaderTdb::cancelOnFail() {
	_proxy = nullptr;
	removeFromQueue();
	_parts.fire({ LoadedPart::kFailedOffset });
}

bool LoaderTdb::haveSentRequests() const {
	return !_waitingOffsets.empty();
}

bool LoaderTdb::haveSentRequestForOffset(int64 offset) const {
	return _waitingOffsets.contains(offset);
}

void LoaderTdb::cancelRequest() {
	if (_requestId) {
		_account->sender().request(base::take(_requestId)).cancel();
	}
	_loadingLifetime.destroy();
}

void LoaderTdb::cancelRequestForOffset(int64 offset) {
	if (_waitingOffsets.remove(offset)) {
		const auto next = _waitingOffsets.empty()
			? _requested.front()
			: _waitingOffsets.front();
		if (next
			&& (_waitingOffsets.empty()
				|| *next < _requestedOffset
				|| _loadedTill + kResendRequestOffset < *next)) {
			addToQueueWithPriority();
		}
	}
}

void LoaderTdb::addToQueue(int priority) {
	const auto downloading = _waitingOffsets.empty()
		? std::optional<int64>()
		: _waitingOffsets.front();
	const auto requesting = _requested.front();
	if (!requesting && !downloading) {
		return;
	}
	const auto newOffset = downloading ? *downloading : *requesting;
	const auto newLimit = std::min(kLoadLimit, _size - newOffset);
	const auto newPriority = kDefaultDownloadPriority + _priority;
	while (!_requested.empty()
		&& *_requested.front() >= newOffset
		&& (*_requested.front() + partSize(*_requested.front())
			<= newOffset + newLimit)) {
		_waitingOffsets.emplace(*_requested.take());
	}
	Assert(!_waitingOffsets.empty());
	const auto needNewRequest = [&] {
		if (newPriority != _requestedPriority
			|| (_requestedOffset + kResendRequestOffset
				< _waitingOffsets.front())) {
			return true;
		}
		for (const auto offset : _waitingOffsets) {
			if (offset < _requestedOffset
				|| (offset + std::min(int64(kPartSize), _size - offset)
					> _requestedOffset + _requestedLimit)) {
				return true;
			}
		}
		return false;
	}();
	if (!needNewRequest) {
		return;
	}
	if (_loadedTill < newOffset || newOffset < _requestedOffset) {
		_loadedTill = newOffset;
	}
	_requestedOffset = newOffset;
	_requestedLimit = newLimit;
	_requestedPriority = newPriority;
	cancelRequest();
	_requestId = _account->sender().request(TLdownloadFile(
		tl_int32(_fileId),
		tl_int32(_requestedPriority),
		tl_int53(_requestedOffset),
		tl_int53(_requestedLimit),
		tl_bool(false)
	)).done([=](const TLfile &result) {
		_requestId = 0;
		apply(result);

		if (!_requestId && haveSentRequests()) {
			_account->updates(
			) | rpl::start_with_next([=](const TLupdate &update) {
				update.match([&](const TLDupdateFile &data) {
					apply(data.vfile());
				}, [](const auto &) {});
			}, _loadingLifetime);
		}
	}).fail([=](const Error &error) {
		cancelOnFail();
	}).send();
}

void LoaderTdb::apply(const TLfile &file) {
	const auto &fields = file.data();
	if (fields.vid().v != _fileId) {
		return;
	}

	const auto &local = fields.vlocal().data();
	const auto downloadedOffset = local.vdownload_offset().v;
	const auto downloadedTill = downloadedOffset
		+ local.vdownloaded_prefix_size().v;
	_loadedTill = std::max(_requestedOffset, downloadedTill);
	_loadingActive = local.vis_downloading_active().v;
	if (!_loadingActive && downloadedTill == downloadedOffset) {
		cancelOnFail();
		return;
	} else if (!_proxy) {
		_proxy = FileProxy::Create(local.vpath().v);
		if (!_proxy) {
			// Maybe the file was moved and we wait for a new updateFile.
			return;
		}
		_proxyPosition = 0;
	}
	const auto was = _waitingOffsets.size();
	const auto weak = base::make_weak(this);
	for (auto i = begin(_waitingOffsets); i != end(_waitingOffsets);) {
		const auto offset = *i;
		const auto limit = partSize(offset);
		if (offset < downloadedOffset || offset + limit > downloadedTill) {
			++i;
			continue;
		} else if (_proxyPosition != offset) {
			if (!_proxy->seek(offset)) {
				cancelOnFail();
				return;
			} else {
				_proxyPosition = offset;
			}
		}
		auto bytes = _proxy->read(limit);
		if (bytes.size() != limit) {
			cancelOnFail();
			return;
		}
		_proxyPosition += limit;
		_parts.fire({ offset, std::move(bytes) });
		if (!weak) {
			return;
		}
		i = _waitingOffsets.erase(i);
	}
	if (!_loadingActive && !_waitingOffsets.empty()) {
		cancelOnFail();
	} else if (_waitingOffsets.size() != was && !_requested.empty()) {
		addToQueueWithPriority();
	} else if (_waitingOffsets.empty()) {
		removeFromQueue();
	}
}

int64 LoaderTdb::partSize(int64 offset) const {
	return std::min(int64(kPartSize), _size - offset);
}

void LoaderTdb::removeFromQueue() {
	if (_requestId || _loadingActive) {
		_account->sender().request(TLcancelDownloadFile(
			tl_int32(_fileId),
			tl_bool(false)
		)).send();
	}
	cancelRequest();
	_requestedOffset = _requestedLimit = _loadedTill = 0;
	_waitingOffsets.clear();
}

rpl::producer<LoadedPart> LoaderTdb::parts() const {
	return _parts.events();
}

} // namespace Streaming
} // namespace Media
