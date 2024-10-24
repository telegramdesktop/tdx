/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "storage/download_manager_mtproto.h"

#include "tdb/tdb_sender.h"

namespace Tdb {
class TLDlocalFile;
} // namespace Tdb

namespace Main {
class Session;
} // namespace Main

namespace Data {

class PhotoMedia;
struct FileOrigin;

class MediaPreload {
public:
	explicit MediaPreload(Fn<void()> done);
	virtual ~MediaPreload() = default;

protected:
	void callDone();

private:
	Fn<void()> _done;

};

class PhotoPreload final : public MediaPreload {
public:
	[[nodiscard]] static bool Should(
		not_null<PhotoData*> photo,
		not_null<PeerData*> context);

	PhotoPreload(
		not_null<PhotoData*> data,
		FileOrigin origin,
		Fn<void()> done);
	~PhotoPreload();

private:
	void start(FileOrigin origin);

	std::shared_ptr<PhotoMedia> _photo;
	rpl::lifetime _lifetime;

};

#if 0 // mtp
class VideoPreload final
	: public MediaPreload
	, private Storage::DownloadMtprotoTask {
public:
	[[nodiscard]] static bool Can(not_null<DocumentData*> video);

	VideoPreload(
		not_null<DocumentData*> video,
		FileOrigin origin,
		Fn<void()> done);
	~VideoPreload();

private:
	void check();
	void load();
	void done(QByteArray result);

	bool readyToRequest() const override;
	int64 takeNextRequestOffset() override;
	bool feedPart(int64 offset, const QByteArray &bytes) override;
	void cancelOnFail() override;
	bool setWebFileSizeHook(int64 size) override;

	const not_null<DocumentData*> _video;
	base::flat_map<uint32, QByteArray> _parts;
	base::flat_set<int> _requestedOffsets;
	int64 _full = 0;
	int _nextRequestOffset = 0;
	bool _finished = false;
	bool _failed = false;

};
#endif

class VideoPreload final
	: public MediaPreload
	, public base::has_weak_ptr {
public:
	[[nodiscard]] static bool Can(not_null<DocumentData*> video);

	VideoPreload(
		not_null<DocumentData*> video,
		Fn<void()> done);
	~VideoPreload();

private:
	void check();
	void load();
	void done(QByteArray result);

	void startWith(const Tdb::TLDlocalFile &data);
	bool continueWith(const Tdb::TLDlocalFile &data);
	void finishWith(const Tdb::TLDlocalFile &data);

	const not_null<DocumentData*> _video;
	const not_null<Main::Session*> _session;
	Tdb::Sender _sender;
	int64 _full = 0;
	FileId _fileId = 0;
	int64 _prefix = 0;

	rpl::lifetime _downloadLifetime;

};

} // namespace Data
