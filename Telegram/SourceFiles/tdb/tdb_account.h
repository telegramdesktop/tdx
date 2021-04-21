/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/details/tdb_instance.h"
#include "tdb/tdb_sender.h"

namespace Tdb {

using AccountConfig = details::InstanceConfig;

class Account final {
public:
	explicit Account(AccountConfig &&config);

	[[nodiscard]] Sender &sender() {
		return _sender;
	}

	[[nodiscard]] rpl::producer<TLupdate> updates() const;

	[[nodiscard]] rpl::lifetime &lifetime() {
		return _lifetime;
	}

private:
	[[nodiscard]] bool consumeUpdate(const TLupdate &update);

	details::Instance _instance;
	Sender _sender;
	rpl::event_stream<TLupdate> _updates;

	rpl::lifetime _lifetime;

};

} // namespace Tdb
