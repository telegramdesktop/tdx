/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/tdb_sender.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtCore/QPointer>

class QNetworkReply;

namespace Tdb {

class Account;

// Testing may be done like that:
//
// request(TLgetRemoteFile(
//	tl_string("http://link.to/image.jpg"),
//	tl_fileTypeDocument()
// )).done([=](const TLfile &result) {
//	request(TLdownloadFile(
//		result.data().vid(),
//		tl_int32(1),
//		tl_int53(0),
//		tl_int53(0),
//		tl_bool(true)
//	)).done([=](const TLfile &result) {
//		const auto path = result.data().vlocal().data().vpath().v;
//	}).send();
// }).send();

class FilesDownloader final {
public:
	FilesDownloader(not_null<Account*> account);
	~FilesDownloader();

	void start(int64 id, const QString &url);
	void finish(int64 id);

private:
	struct Enqueued;
	struct Sent;

	void sendNext();
	[[nodiscard]] not_null<QNetworkReply*> send(
		int64 id,
		const QString &url);

	void removeSent(int64 id);
	void deleteDeferred(not_null<QNetworkReply*> reply);
	[[nodiscard]] Sent *findSent(int64 id, not_null<QNetworkReply*> reply);

	void read(int64 id, not_null<QNetworkReply*> reply);
	void read(int64 id, not_null<Sent*> sent);
	void written(int64 id, not_null<QNetworkReply*> reply, RequestId rid);
	void finished(int64 id, not_null<QNetworkReply*> reply);
	void redirect(int id, not_null<QNetworkReply*> reply);
	void failed(int64 id, not_null<QNetworkReply*> reply, int error);
	void failed(int64 id, not_null<QNetworkReply*> reply);

	const not_null<Account*> _account;
	Sender _sender;

	base::flat_map<int64, Enqueued> _enqueued;
	base::flat_map<int64, Sent> _sent;

	std::vector<QPointer<QNetworkReply>> _repliesBeingDeleted;

	QNetworkAccessManager _manager;

};

} // namespace Tdb
