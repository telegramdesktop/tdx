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

class Websites final {
public:
	explicit Websites(not_null<ApiWrap*> api);

	struct Entry {
#if 0 // mtp
		uint64 hash = 0;
#endif
		int64 hash = 0;

		not_null<UserData*> bot;
		TimeId activeTime = 0;
		QString active, platform, domain, browser, ip, location;
	};
	using List = std::vector<Entry>;

	void reload();
	void cancelCurrentRequest();
	void requestTerminate(
#if 0 // mtp
		Fn<void(const MTPBool &result)> &&done,
		Fn<void(const MTP::Error &error)> &&fail,
#endif
		Fn<void()> &&done,
		Fn<void()> &&fail,
		std::optional<uint64> hash = std::nullopt,
		UserData *botToBlock = nullptr);

	[[nodiscard]] crl::time lastReceivedTime();

	[[nodiscard]] List list() const;
	[[nodiscard]] rpl::producer<List> listValue() const;
	[[nodiscard]] int total() const;
	[[nodiscard]] rpl::producer<int> totalValue() const;

private:
	not_null<Main::Session*> _session;

	MTP::Sender _api;
	mtpRequestId _requestId = 0;

	List _list;
	rpl::event_stream<> _listChanges;

	crl::time _lastReceived = 0;
	rpl::lifetime _lifetime;

};

} // namespace Api
