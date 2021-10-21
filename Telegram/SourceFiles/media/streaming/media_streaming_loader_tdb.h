/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "media/streaming/media_streaming_loader.h"
#include "storage/cache/storage_cache_types.h"
#include "tdb/tdb_request_id.h"

namespace Tdb {
class Account;
class FileProxy;
class TLfile;
} // namespace Tdb

namespace Media {
namespace Streaming {

class LoaderTdb : public Loader, public base::has_weak_ptr {
public:
	LoaderTdb(
		not_null<Tdb::Account*> account,
		FileId fileId,
		Storage::Cache::Key baseCacheKey,
		int size);
	~LoaderTdb();

	[[nodiscard]] Storage::Cache::Key baseCacheKey() const override;
	[[nodiscard]] int size() const override;

	void load(int offset) override;
	void cancel(int offset) override;
	void resetPriorities() override;
	void setPriority(int priority) override;
	void stop() override;

	void tryRemoveFromQueue() override;

	// Parts will be sent from the main thread.
	[[nodiscard]] rpl::producer<LoadedPart> parts() const override;

	void attachDownloader(
		not_null<Storage::StreamedFileDownloader*> downloader) override;
	void clearAttachedDownloader() override;

private:
	[[nodiscard]] bool haveSentRequests() const;
	[[nodiscard]] bool haveSentRequestForOffset(int offset) const;
	void cancelRequestForOffset(int offset);
	void cancelRequest();

	void addToQueue(int priority = 0);
	void removeFromQueue();

	void cancelForOffset(int offset);
	void addToQueueWithPriority();
	void cancelOnFail();
	void apply(const Tdb::TLfile &file);
	[[nodiscard]] int partSize(int offset) const;

	const not_null<Tdb::Account*> _account;
	const FileId _fileId = 0;
	const Storage::Cache::Key _baseCacheKey;
	const int _size = 0;
	int _priority = 0;

	Tdb::RequestId _requestId = 0;
	int _requestedOffset = 0;
	int _requestedLimit = 0;
	int _requestedPriority = 0;
	int _loadedTill = 0;
	base::flat_set<int> _waitingOffsets;
	rpl::lifetime _loadingLifetime;
	std::unique_ptr<Tdb::FileProxy> _proxy;
	int _proxyPosition = 0;

	PriorityQueue _requested;
	rpl::event_stream<LoadedPart> _parts;

	Storage::StreamedFileDownloader *_downloader = nullptr;

};

} // namespace Streaming
} // namespace Media
