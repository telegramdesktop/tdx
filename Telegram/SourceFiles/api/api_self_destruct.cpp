/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_self_destruct.h"

#include "apiwrap.h"

#include "tdb/tdb_tl_scheme.h"

namespace Api {

SelfDestruct::SelfDestruct(not_null<ApiWrap*> api)
: _api(&api->instance()) {
}

#if 0 // goodToRemove
void SelfDestruct::reload() {
	if (!_accountTTL.requestId) {
		_accountTTL.requestId = _api.request(MTPaccount_GetAccountTTL(
		)).done([=](const MTPAccountDaysTTL &result) {
			_accountTTL.requestId = 0;
			_accountTTL.days = result.data().vdays().v;
		}).fail([=] {
			_accountTTL.requestId = 0;
		}).send();
	}
	if (!_defaultHistoryTTL.requestId) {
		_defaultHistoryTTL.requestId = _api.request(
			MTPmessages_GetDefaultHistoryTTL()
		).done([=](const MTPDefaultHistoryTTL &result) {
			_defaultHistoryTTL.requestId = 0;
			_defaultHistoryTTL.period = result.data().vperiod().v;
		}).fail([=] {
			_defaultHistoryTTL.requestId = 0;
		}).send();
	}
}
#endif

void SelfDestruct::reload() {
	using namespace Tdb;
	if (!_accountTTL.requestId) {
		_accountTTL.requestId = _api.request(TLgetAccountTtl(
		)).done([=](const TLDaccountTtl &data) {
			_accountTTL.requestId = 0;
			_accountTTL.days = data.vdays().v;
		}).fail([=](const Error &error) {
			_accountTTL.requestId = 0;
		}).send();
	}
	if (!_defaultHistoryTTL.requestId) {
		_defaultHistoryTTL.requestId = _api.request(
			TLgetDefaultMessageAutoDeleteTime()
		).done([=](const TLDmessageAutoDeleteTime &data) {
			_defaultHistoryTTL.requestId = 0;
			_defaultHistoryTTL.period = data.vtime().v;
		}).fail([=](const Error &error) {
			_defaultHistoryTTL.requestId = 0;
		}).send();
	}
}

rpl::producer<int> SelfDestruct::daysAccountTTL() const {
	return _accountTTL.days.value() | rpl::filter(rpl::mappers::_1 != 0);
}

rpl::producer<TimeId> SelfDestruct::periodDefaultHistoryTTL() const {
	return _defaultHistoryTTL.period.value();
}

TimeId SelfDestruct::periodDefaultHistoryTTLCurrent() const {
	return _defaultHistoryTTL.period.current();
}

#if 0 // goodToRemove
void SelfDestruct::updateAccountTTL(int days) {
	_api.request(_accountTTL.requestId).cancel();
	_accountTTL.requestId = _api.request(MTPaccount_SetAccountTTL(
		MTP_accountDaysTTL(MTP_int(days))
	)).done([=] {
		_accountTTL.requestId = 0;
	}).fail([=] {
		_accountTTL.requestId = 0;
	}).send();
	_accountTTL.days = days;
}

void SelfDestruct::updateDefaultHistoryTTL(TimeId period) {
	_api.request(_defaultHistoryTTL.requestId).cancel();
	_defaultHistoryTTL.requestId = _api.request(
		MTPmessages_SetDefaultHistoryTTL(MTP_int(period))
	).done([=] {
		_defaultHistoryTTL.requestId = 0;
	}).fail([=] {
		_defaultHistoryTTL.requestId = 0;
	}).send();
	_defaultHistoryTTL.period = period;
}
#endif

void SelfDestruct::updateAccountTTL(int days) {
	using namespace Tdb;
	_accountTTL.requestId = _api.request(TLsetAccountTtl(
		tl_accountTtl(tl_int32(days))
	)).done([=](const TLok &result) {
		_accountTTL.requestId = 0;
	}).fail([=](const Error &result) {
		_accountTTL.requestId = 0;
	}).send();
	_accountTTL.days = days;
}

void SelfDestruct::updateDefaultHistoryTTL(TimeId period) {
	using namespace Tdb;
	_defaultHistoryTTL.requestId = _api.request(
		TLsetDefaultMessageAutoDeleteTime(
			tl_messageAutoDeleteTime(tl_int32(period)))
	).done([=](const TLok &result) {
		_defaultHistoryTTL.requestId = 0;
	}).fail([=](const Error &result) {
		_defaultHistoryTTL.requestId = 0;
	}).send();
	_defaultHistoryTTL.period = period;
}

} // namespace Api
