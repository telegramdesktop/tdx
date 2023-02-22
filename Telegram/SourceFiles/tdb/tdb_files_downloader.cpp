/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/tdb_files_downloader.h"

#include "base/debug_log.h"
#include "tdb/tdb_account.h"

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QAuthenticator>

namespace Tdb {
namespace {

constexpr auto kMaxQueries = 8;
constexpr auto kMaxHttpRedirects = 5;
constexpr auto kChunk = 128 * 1024;
constexpr auto kChunksInBuffer = 8;

} // namespace

struct FilesDownloader::Enqueued {
	QString url;
};

struct FilesDownloader::Sent {
	QString url;
	not_null<QNetworkReply*> reply;
	QByteArray data;
	base::flat_set<RequestId> requests;
	int64 sent = 0;
	int redirectsLeft = kMaxHttpRedirects;
	bool downloaded = false;
};

FilesDownloader::FilesDownloader(not_null<Account*> account)
: _account(account)
, _sender(&_account->sender()) {
	const auto fail = [=](QNetworkReply *reply) {
		for (const auto &[id, sent] : _sent) {
			if (sent.reply == reply) {
				failed(id, reply);
				return;
			}
		}
	};
	QObject::connect(
		&_manager,
		&QNetworkAccessManager::authenticationRequired,
		fail);
	QObject::connect(
		&_manager,
		&QNetworkAccessManager::sslErrors,
		fail);
}

FilesDownloader::~FilesDownloader() {
	for (const auto &[id, sent] : base::take(_sent)) {
		sent.reply->abort();
		delete sent.reply;
	}
	for (const auto &reply : base::take(_repliesBeingDeleted)) {
		if (reply) {
			delete reply;
		}
	}
}

void FilesDownloader::start(int64 id, const QString &url) {
	_enqueued.emplace(id, Enqueued{ url });
	sendNext();
}

void FilesDownloader::finish(int64 id) {
	_enqueued.erase(id);
	removeSent(id);
}

void FilesDownloader::removeSent(int64 id) {
	if (const auto i = _sent.find(id); i != end(_sent)) {
		deleteDeferred(i->second.reply);
		_sent.erase(i);
		sendNext();
	}
}

void FilesDownloader::deleteDeferred(not_null<QNetworkReply*> reply) {
	reply->deleteLater();
	_repliesBeingDeleted.erase(
		ranges::remove(_repliesBeingDeleted, nullptr),
		end(_repliesBeingDeleted));
	_repliesBeingDeleted.emplace_back(reply.get());
}

void FilesDownloader::sendNext() {
	while (!_enqueued.empty() && _sent.size() < kMaxQueries) {
		const auto i = _enqueued.begin();
		const auto id = i->first;
		const auto url = i->second.url;
		_enqueued.erase(i);

		_sent.emplace(id, Sent{ .url = url, .reply = send(id, url) });
	}
}

not_null<QNetworkReply*> FilesDownloader::send(
		int64 id,
		const QString &url) {
	const auto reply = _manager.get(QNetworkRequest(url));
	reply->setReadBufferSize(kChunk * kChunksInBuffer);

	const auto handleReadyRead = [=] {
		read(id, reply);
	};
	const auto handleError = [=](QNetworkReply::NetworkError error) {
		failed(id, reply, error);
	};
	const auto handleFinished = [=] {
		finished(id, reply);
	};
	QObject::connect(reply, &QNetworkReply::readyRead, handleReadyRead);
	QObject::connect(reply, &QNetworkReply::errorOccurred, handleError);
	QObject::connect(reply, &QNetworkReply::finished, handleFinished);

	return reply;
}

void FilesDownloader::read(int64 id, not_null<QNetworkReply*> reply) {
	if (const auto sent = findSent(id, reply)) {
		read(id, sent);
	}
}

void FilesDownloader::read(int64 id, not_null<Sent*> sent) {
	while (true) {
		const auto reply = sent->reply;
		const auto read = reply->read(kChunk);
		if (read.isEmpty() && (!sent->downloaded || sent->data.isEmpty())) {
			break;
		} else if (!sent->data.isEmpty()) {
			sent->data.append(read);
		} else {
			sent->data = read;
		}
		const auto chunkSize = sent->downloaded
			? std::min(int(sent->data.size()), kChunk)
			: kChunk;
		Assert(chunkSize > 0);
		while (sent->data.size() >= chunkSize) {
			const auto exact = (sent->data.size() == chunkSize);
			const auto rid = _sender.request(TLwriteGeneratedFilePart(
				tl_int64(id),
				tl_int53(sent->sent),
				tl_bytes(exact ? sent->data : sent->data.mid(0, chunkSize))
			)).done([=](const TLok &, RequestId rid) {
				written(id, reply, rid);
			}).fail([=](const Error &error) {
				finish(id);
			}).send();

			sent->sent += chunkSize;
			if (exact) {
				sent->data = QByteArray();
			} else {
				sent->data = sent->data.mid(chunkSize);
			}
			sent->requests.emplace(rid);
		}
	}
}

void FilesDownloader::written(int64 id, not_null<QNetworkReply*> reply, RequestId rid) {
	if (const auto sent = findSent(id, reply)) {
		sent->requests.erase(rid);
		if (sent->downloaded && sent->requests.empty()) {
			_sender.request(TLfinishFileGeneration(
				tl_int64(id),
				std::nullopt
			)).send();
			finish(id);
		}
	}
}

void FilesDownloader::finished(int64 id, not_null<QNetworkReply*> reply) {
	const auto statusCode = reply->attribute(
		QNetworkRequest::HttpStatusCodeAttribute);
	const auto status = statusCode.isValid() ? statusCode.toInt() : 200;
	if (status == 301 || status == 302) {
		redirect(id, reply);
	} else if (status != 200 && status != 206 && status != 416) {
		LOG(("Network Error: "
			"Bad HTTP status received in FilesDownloader::finished() %1"
			).arg(status));
		failed(id, reply);
	} else if (const auto sent = findSent(id, reply)) {
		sent->downloaded = true;
		read(id, reply);
	}
}

FilesDownloader::Sent *FilesDownloader::findSent(
		int64 id,
		not_null<QNetworkReply*> reply) {
	const auto i = _sent.find(id);
	return (i != end(_sent) && i->second.reply == reply)
		? &i->second
		: nullptr;
}

void FilesDownloader::failed(
		int64 id,
		not_null<QNetworkReply*> reply,
		int error) {
	if (const auto sent = findSent(id, reply)) {
		LOG(("Network Error: "
			"Failed to request '%1', error %2 (%3)"
			).arg(sent->url
			).arg(error
			).arg(reply->errorString()));
		failed(id, reply);
	}
}

void FilesDownloader::failed(int64 id, not_null<QNetworkReply*> reply) {
	if (const auto sent = findSent(id, reply)) {
		_sender.request(TLfinishFileGeneration(
			tl_int64(id),
			tl_error(tl_int32(0), tl_string("download error"))
		)).send();
		removeSent(id);
	}
}

void FilesDownloader::redirect(int id, not_null<QNetworkReply*> reply) {
	const auto sent = findSent(id, reply);
	if (!sent) {
		return;
	}

	const auto header = reply->header(QNetworkRequest::LocationHeader);
	const auto url = header.toString();
	if (url.isEmpty()) {
		LOG(("Network Error: "
			"Empty HTTP redirect url for downloader: %1").arg(sent->url));
		failed(id, reply);
		return;
	} else if (sent->sent > 0 || !sent->requests.empty()) {
		LOG(("Network Error: "
			"HTTP redirect in the middle of downloader: %1").arg(sent->url));
		failed(id, reply);
		return;
	} else if (!sent->redirectsLeft--) {
		LOG(("Network Error: "
			"Too many HTTP redirects in downloader: %1").arg(sent->url));
		failed(id, reply);
		return;
	}
	deleteDeferred(reply);
	sent->url = url;
	sent->reply = send(id, url);
}

} // namespace Tdb
