/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/sender.h"

class ApiWrap;

namespace Main {
class Session;
} // namespace Main

namespace Api {

class BlockedPeers final {
public:
	struct Slice {
		struct Item {
			PeerId id;
			TimeId date = 0;

			bool operator==(const Item &other) const;
			bool operator!=(const Item &other) const;
		};

		QVector<Item> list;
		int total = 0;

		bool operator==(const Slice &other) const;
		bool operator!=(const Slice &other) const;
	};

	explicit BlockedPeers(not_null<ApiWrap*> api);

#if 0 // goodToRemove
	void reload();
	rpl::producer<Slice> slice();
#endif
	void request(int offset, Fn<void(Slice)> done);

	void block(not_null<PeerData*> peer);
	void unblock(
		not_null<PeerData*> peer,
		Fn<void(bool success)> done = nullptr,
		bool force = false);

	rpl::producer<int> total();

private:
	struct Request {
		std::vector<Fn<void(bool success)>> callbacks;
		mtpRequestId requestId = 0;
		bool blocking = false;
	};

	[[nodiscard]] bool blockAlreadySent(
		not_null<PeerData*> peer,
		bool blocking,
		Fn<void(bool success)> done = nullptr);

	void changeBlockState(
		not_null<PeerData*> peer,
		bool blocked,
		bool force = false,
		Fn<void(bool success)> done = nullptr);

	const not_null<Main::Session*> _session;

	MTP::Sender _api;
	base::flat_map<not_null<PeerData*>, Request> _blockRequests;
	int _total = 0;

#if 0 // goodToRemove
	mtpRequestId _requestId = 0;
	std::optional<Slice> _slice;
	rpl::event_stream<Slice> _changes;
#endif


};

} // namespace Api
