/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "storage/file_download.h"
#include "tdb/tdb_request_id.h"

namespace Tdb {
class TLfile;
} // namespace Tdb

class TdbFileLoader final : public FileLoader {
public:
	TdbFileLoader(
		not_null<Main::Session*> session,
		FileId fileId,
		LocationType type,
		const QString &toFile,
		int loadSize,
		int fullSize,
		LoadToCacheSetting toCache,
		LoadFromCloudSetting fromCloud,
		bool autoLoading,
		uint8 cacheTag);
	~TdbFileLoader();

	[[nodiscard]] FileId fileId() const;

	int currentOffset() const override;

private:
	class FileProxy;

	Storage::Cache::Key cacheKey() const override;
	std::optional<MediaKey> fileLocationKey() const override;
	void startLoading() override;
	void startLoadingWithPartial(const QByteArray &data) override;
	void cancelHook() override;
	void increaseLoadSizeHook() override;

	void sendRequest();
	void cancelRequest();

	void apply(const Tdb::TLfile &file);
	bool setFinalSize(int size);
	bool feedPart(int offset, const QByteArray &bytes);

	[[nodiscard]] static std::unique_ptr<FileProxy> CreateFileProxy(
		const QString &path);

	const FileId _fileId = 0;
	int _loadOffset = 0;
	std::unique_ptr<FileProxy> _proxy;
	Tdb::RequestId _requestId = 0;

	rpl::lifetime _loadingLifetime;

};
