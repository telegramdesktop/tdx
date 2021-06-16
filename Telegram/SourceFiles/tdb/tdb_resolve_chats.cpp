/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/tdb_resolve_chats.h"

namespace Tdb {
namespace {

constexpr auto kUserShift = 0x0100000000000000ULL;
constexpr auto kChatShift = 0x0200000000000000ULL;
constexpr auto kChannelShift = 0x0400000000000000ULL;

[[nodiscard]] uint64 PeerFromChatId(TLint53 id) noexcept {
	//constexpr int64 MIN_SECRET_ID = -2002147483648ll; // From TDLib.
	//constexpr int64 ZERO_SECRET_ID = -2000000000000ll;
	//constexpr int64 MAX_SECRET_ID = -1997852516353ll;
	constexpr int64 MIN_CHANNEL_ID = -1002147483647ll;
	constexpr int64 MAX_CHANNEL_ID = -1000000000000ll;
	constexpr int64 MIN_CHAT_ID = -2147483647ll;
	//constexpr int64 MAX_USER_ID = 2147483647ll;
	if (id.v > 0) {
		return kUserShift | uint64(id.v);
	} else if (id.v < 0 && id.v > MIN_CHAT_ID) {
		return kChatShift | uint64(-id.v);
	} else if (id.v < MAX_CHANNEL_ID && id.v > MIN_CHANNEL_ID) {
		return kChannelShift | uint64(MAX_CHANNEL_ID - id.v);
	}
	return 0;
}

[[nodiscard]] uint64 PeerToUser(uint64 peer) {
	return (peer & kUserShift) ? (peer & ~kUserShift) : 0;
}

[[nodiscard]] uint64 PeerToChat(uint64 peer) {
	return (peer & kChatShift) ? (peer & ~kChatShift) : 0;
}

[[nodiscard]] uint64 PeerToChannel(uint64 peer) {
	return (peer & kChannelShift) ? (peer & ~kChannelShift) : 0;
}

} // namespace

ResolveChatsRequest::~ResolveChatsRequest() {
	cancel();
}

void ResolveChatsRequest::cancel() {
	if (!_data) {
		return;
	}
	const auto data = base::take(_data);
	for (const auto id : data->requests) {
		data->sender.request(id).cancel();
	}
}

void ResolveChatsRequest::listFail(const Error &error) {
	const auto data = base::take(_data);
	if (data->fail) {
		data->fail(error);
	}
}

void ResolveChatsRequest::listDone(const TLchats &chats) {
	Expects(_data != nullptr);

	chats.match([&](const TLDchats &data) {
		const auto &ids = data.vchat_ids().v;
		_data->result.count = data.vtotal_count().v;
		_data->result.ids = data.vchat_ids().v;
		for (const auto &chatId : ids) {
			requestChat(chatId);
		}
	});
}

void ResolveChatsRequest::requestChat(TLint53 chatId) {
	const auto peer = PeerFromChatId(chatId);
	if (!peer) {
		return;
	}
	_data->requests.emplace(_data->sender.request(TLgetChat(
		chatId
	)).done([self = _data->self](const TLchat &result, RequestId requestId) {
		if (const auto that = self()) {
			that->_data->result.dialogs.push_back(result);
			that->requestFinished(requestId);
		}
	}).fail([self = _data->self](const Error &error, RequestId requestId) {
		if (const auto that = self()) {
			that->requestFinished(requestId);
		}
	}).send());

	requestPeer(peer);
}

void ResolveChatsRequest::requestPeer(uint64 peer) {
	if (!_data->requestedPeers.emplace(peer).second) {
		return;
	}

	if (const auto user = PeerToUser(peer)) {
		_data->requests.emplace(_data->sender.request(TLgetUser(
			tl_int53(user)
		)).done([self = _data->self](const TLuser &result, RequestId requestId) {
			if (const auto that = self()) {
				that->_data->result.users.push_back(result);
				that->requestFinished(requestId);
			}
		}).fail([self = _data->self](const Error &error, RequestId requestId) {
			if (const auto that = self()) {
				that->requestFinished(requestId);
			}
		}).send());
	} else if (const auto chat = PeerToChat(peer)) {
		_data->requests.emplace(_data->sender.request(TLgetBasicGroup(
			tl_int53(chat)
		)).done([self = _data->self](const TLbasicGroup &result, RequestId requestId) {
			if (const auto that = self()) {
				that->_data->result.chats.push_back(result);
				that->requestFinished(requestId);
			}
		}).fail([self = _data->self](const Error &error, RequestId requestId) {
			if (const auto that = self()) {
				that->requestFinished(requestId);
			}
		}).send());
	} else if (const auto channel = PeerToChannel(peer)) {
		_data->requests.emplace(_data->sender.request(TLgetSupergroup(
			tl_int53(channel)
		)).done([self = _data->self](const TLsupergroup &result, RequestId requestId) {
			if (const auto that = self()) {
				that->_data->result.channels.push_back(result);
				that->requestFinished(requestId);
			}
		}).fail([self = _data->self](const Error &error, RequestId requestId) {
			if (const auto that = self()) {
				that->requestFinished(requestId);
			}
		}).send());
	}
}

void ResolveChatsRequest::requestFinished(RequestId requestId) {
	Expects(_data != nullptr);

	const auto removed = _data->requests.remove(requestId);
	Assert(removed);
	if (!_data->requests.empty()) {
		return;
	}
	const auto data = base::take(_data);
	if (data->done) {
		data->done(data->result);
	}
}

} // namespace Tdb
