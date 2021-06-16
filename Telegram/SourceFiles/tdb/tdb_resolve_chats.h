/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/tdb_sender.h"
#include "tdb/tdb_tl_scheme.h"

namespace Tdb {

struct ResolvedChats {
	QVector<TLint53> ids;
	std::vector<TLchat> dialogs;
	std::vector<TLuser> users;
	std::vector<TLbasicGroup> chats;
	std::vector<TLsupergroup> channels;
	int count = 0;
};

class ResolveChatsRequest final {
public:
	ResolveChatsRequest() = default;
	ResolveChatsRequest(ResolveChatsRequest &&) = default;
	ResolveChatsRequest &operator=(ResolveChatsRequest &&) = default;
	~ResolveChatsRequest();

	[[nodiscard]] explicit operator bool() const {
		return _data != nullptr;
	}

	template <typename Request>
	void send(
			Sender &sender,
			Request &&request,
			Fn<ResolveChatsRequest*()> self,
			FnMut<void(const ResolvedChats &)> done,
			FnMut<void(const Error &error)> fail = nullptr) {
		const auto id = sender.request(
			std::move(request)
		).done([self](const TLchats &chats, RequestId requestId) {
			if (const auto that = self()) {
				that->listDone(chats);
				that->requestFinished(requestId);
			}
		}).fail([self](const Error &error) {
			if (const auto that = self()) {
				that->listFail(error);
			}
		}).send();

		cancel();
		_data = std::make_unique<Data>(
			sender,
			self,
			id,
			std::move(done),
			std::move(fail));
	}

	void cancel();

private:
	struct Data {
		Data(
			Sender &sender,
			Fn<ResolveChatsRequest*()> self,
			RequestId id,
			FnMut<void(const ResolvedChats &)> done,
			FnMut<void(const Error &)> fail)
		: sender(sender)
		, self(std::move(self))
		, requests{ &id, &id + 1 }
		, done(std::move(done))
		, fail(std::move(fail)) {
		}

		Sender &sender;
		Fn<ResolveChatsRequest*()> self;
		base::flat_set<uint64> requestedPeers;
		base::flat_set<RequestId> requests;
		ResolvedChats result;
		FnMut<void(const ResolvedChats &)> done;
		FnMut<void(const Error &)> fail;
	};

	void listFail(const Error &error);
	void listDone(const TLchats &chats);
	void requestChat(TLint53 chatId);
	void requestPeer(uint64 peer);
	void requestFinished(RequestId requestId);

	std::unique_ptr<Data> _data;

};

} // namespace Tdb
