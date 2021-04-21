/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/tdb_account.h"

namespace Tdb {

Account::Account(AccountConfig &&config)
: _instance(std::move(config))
, _sender(&_instance) {
	_instance.updates(
	) | rpl::start_with_next([=](TLupdate &&update) {
		if (!consumeUpdate(update)) {
			_updates.fire(std::move(update));
		}
	}, _lifetime);
}

rpl::producer<TLupdate> Account::updates() const {
	return _updates.events();
}

bool Account::consumeUpdate(const TLupdate &update) {
	return update.match([&](const TLDupdateAuthorizationState &data) {
		return data.vauthorization_state().match([&](
				const TLDauthorizationStateWaitEncryptionKey &data) {
			_sender.request(
				TLcheckDatabaseEncryptionKey(tl_bytes())
			).send();
			return true;
		}, [&](const auto &) {
			return false;
		});
	}, [](const auto &) {
		return false;
	});
}

} // namespace Tdb
