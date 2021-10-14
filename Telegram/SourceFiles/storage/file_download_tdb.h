/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "storage/file_download.h"

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
	Storage::Cache::Key cacheKey() const override;
	std::optional<MediaKey> fileLocationKey() const override;
	void startLoading() override;
	void startLoadingWithPartial(const QByteArray &data) override;
	void cancelHook() override;

	void apply(const Tdb::TLfile &file);
	bool setFinalSize(int size);
	bool feedPart(int offset, const QByteArray &bytes);

	const FileId _fileId = 0;
	QFile _reading;
	bool _loading = false;
	int _loadOffset = 0;
	int _ready = 0;

	rpl::lifetime _lifetime;

};
