/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_authorizations.h"

#include "apiwrap.h"
#include "base/unixtime.h"
#include "core/changelogs.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_keys.h"

namespace Api {
namespace {

using namespace Tdb;

constexpr auto TestApiId = 17349;
constexpr auto SnapApiId = 611335;
constexpr auto DesktopApiId = 2040;

#if 0 // goodToRemove
Authorizations::Entry ParseEntry(const MTPDauthorization &data) {
	auto result = Authorizations::Entry();

	result.hash = data.is_current() ? 0 : data.vhash().v;
	result.incomplete = data.is_password_pending();
	result.callsDisabled = data.is_call_requests_disabled();

	const auto apiId = result.apiId = data.vapi_id().v;
	const auto isTest = (apiId == TestApiId);
	const auto isDesktop = (apiId == DesktopApiId)
		|| (apiId == SnapApiId)
		|| isTest;

	const auto appName = isDesktop
		? u"Telegram Desktop%1"_q.arg(isTest ? " (GitHub)" : QString())
		: qs(data.vapp_name());// + u" for "_q + qs(d.vplatform());
	const auto appVer = [&] {
		const auto version = qs(data.vapp_version());
		if (isDesktop) {
			const auto verInt = version.toInt();
			if (version == QString::number(verInt)) {
				return Core::FormatVersionDisplay(verInt);
			}
		} else {
			if (const auto index = version.indexOf('('); index >= 0) {
				return version.mid(index);
			}
		}
		return version;
	}();

	result.name = result.hash
		? qs(data.vdevice_model())
		: Core::App().settings().deviceModel();

	const auto country = qs(data.vcountry());
	//const auto platform = qs(data.vplatform());
	//const auto &countries = countriesByISO2();
	//const auto j = countries.constFind(country);
	//if (j != countries.cend()) {
	//	country = QString::fromUtf8(j.value()->name);
	//}
	result.system = qs(data.vsystem_version());
	result.platform = qs(data.vplatform());
	result.activeTime = data.vdate_active().v
		? data.vdate_active().v
		: data.vdate_created().v;
	result.info = QString("%1%2").arg(
		appName,
		appVer.isEmpty() ? QString() : (' ' + appVer));
	result.ip = qs(data.vip());
	result.active = result.hash
		? Authorizations::ActiveDateString(result.activeTime)
		: tr::lng_status_online(tr::now);
	result.location = country;

	return result;
}
#endif

Authorizations::Entry ParseEntry(const Tdb::TLDsession &data) {
	auto result = Authorizations::Entry();

	result.hash = data.vis_current().v ? 0 : data.vid().v;
	result.incomplete = data.vis_password_pending().v;

	const auto apiId = data.vapi_id().v;
	const auto isTest = (apiId == TestApiId);
	const auto isDesktop = (apiId == DesktopApiId) || isTest;

	const auto appName = isDesktop
		? QString("Telegram Desktop%1").arg(isTest ? " (GitHub)" : QString())
		: data.vapplication_name().v;
	const auto appVer = [&] {
		const auto version = data.vapplication_version().v;
		if (isDesktop) {
			const auto verInt = version.toInt();
			if (version == QString::number(verInt)) {
				return Core::FormatVersionDisplay(verInt);
			}
		} else {
			if (const auto index = version.indexOf('('); index >= 0) {
				return version.mid(index);
			}
		}
		return version;
	}();

	result.name = QString("%1%2").arg(
		appName,
		appVer.isEmpty() ? QString() : (' ' + appVer));

	const auto country = data.vcountry().v;
	const auto platform = data.vplatform().v;

	result.activeTime = data.vlast_active_date().v
		? data.vlast_active_date().v
		: data.vlog_in_date().v;
	result.info = QString("%1, %2%3").arg(
		data.vdevice_model().v,
		platform.isEmpty() ? QString() : platform + ' ',
		data.vsystem_version().v);
	result.ip = data.vip().v
		+ (country.isEmpty()
			? QString()
			: QString::fromUtf8(" \xe2\x80\x93 ") + country);
	if (!result.hash) {
		result.active = tr::lng_status_online(tr::now);
	} else {
		const auto now = QDateTime::currentDateTime();
		const auto lastTime = base::unixtime::parse(result.activeTime);
		const auto nowDate = now.date();
		const auto lastDate = lastTime.date();
		if (lastDate == nowDate) {
			result.active = lastTime.toString(cTimeFormat());
		} else if (lastDate.year() == nowDate.year()
			&& lastDate.weekNumber() == nowDate.weekNumber()) {
			result.active = langDayOfWeek(lastDate);
		} else {
			result.active = lastDate.toString(qsl("d.MM.yy"));
		}
	}

	return result;
}

} // namespace

Authorizations::Authorizations(not_null<ApiWrap*> api)
: _api(&api->instance()) {
	Core::App().settings().deviceModelChanges(
	) | rpl::start_with_next([=](const QString &model) {
		auto changed = false;
		for (auto &entry : _list) {
			if (!entry.hash) {
				entry.name = model;
				changed = true;
			}
		}
		if (changed) {
			_listChanges.fire({});
		}
	}, _lifetime);

	if (Core::App().settings().disableCallsLegacy()) {
		toggleCallsDisabledHere(true);
	}
}

void Authorizations::reload() {
	if (_requestId) {
		return;
	}

#if 0 // goodToRemove
	_requestId = _api.request(MTPaccount_GetAuthorizations(
	)).done([=](const MTPaccount_Authorizations &result) {
		_requestId = 0;
		_lastReceived = crl::now();
		const auto &data = result.data();
		_ttlDays = data.vauthorization_ttl_days().v;
		_list = ranges::views::all(
			data.vauthorizations().v
		) | ranges::views::transform([](const MTPAuthorization &auth) {
			return ParseEntry(auth.data());
		}) | ranges::to<List>;
		refreshCallsDisabledHereFromCloud();
		_listChanges.fire({});
	}).fail([=] {
		_requestId = 0;
	}).send();
#endif

	using namespace Tdb;
	_requestId = _api.request(TLgetActiveSessions(
	)).done([=](const TLDsessions &data) {
		_requestId = 0;
		_lastReceived = crl::now();
		_list = (
			data.vsessions().v
		) | ranges::views::transform([](const TLsession &d) {
			return ParseEntry(d.c_session());
		}) | ranges::to<List>;
		refreshCallsDisabledHereFromCloud();
		_listChanges.fire({});
	}).fail([=](const Error &error) {
		_requestId = 0;
	}).send();
}

void Authorizations::cancelCurrentRequest() {
#if 0 // goodToRemove
	_api.request(base::take(_requestId)).cancel();
#endif
}

void Authorizations::refreshCallsDisabledHereFromCloud() {
	const auto that = ranges::find(_list, 0, &Entry::hash);
	if (that != end(_list)
		&& !_toggleCallsDisabledRequests.contains(0)) {
		_callsDisabledHere = that->callsDisabled;
	}
}

#if 0 // goodToRemove
void Authorizations::requestTerminate(
		Fn<void(const MTPBool &result)> &&done,
		Fn<void(const MTP::Error &error)> &&fail,
		std::optional<uint64> hash) {
	const auto send = [&](auto request) {
		_api.request(
			std::move(request)
		).done([=, done = std::move(done)](const MTPBool &result) {
			done(result);
			if (mtpIsTrue(result)) {
				if (hash) {
					_list.erase(
						ranges::remove(_list, *hash, &Entry::hash),
						end(_list));
				} else {
					_list.clear();
				}
				_listChanges.fire({});
			}
		}).fail(
			std::move(fail)
		).send();
	};
	if (hash) {
		send(MTPaccount_ResetAuthorization(MTP_long(*hash)));
	} else {
		send(MTPauth_ResetAuthorizations());
	}
}
#endif

void Authorizations::requestTerminate(
		Fn<void()> &&done,
		Fn<void()> &&fail,
		std::optional<uint64> hash) {
	const auto send = [&](auto request) {
		_api.request(
			std::move(request)
		).done([=, done = std::move(done)] {
			done();
			if (hash) {
				_list.erase(
					ranges::remove(_list, *hash, &Entry::hash),
					end(_list));
			} else {
				_list.clear();
			}
			_listChanges.fire({});
		}).fail(
			std::move(fail)
		).send();
	};
	if (hash) {
		send(Tdb::TLterminateSession(Tdb::tl_int64(*hash)));
	} else {
		send(Tdb::TLterminateAllOtherSessions());
	}
}

Authorizations::List Authorizations::list() const {
	return _list;
}

auto Authorizations::listValue() const
-> rpl::producer<Authorizations::List> {
	return rpl::single(
		list()
	) | rpl::then(
		_listChanges.events() | rpl::map([=] { return list(); })
	);
}

rpl::producer<int> Authorizations::totalValue() const {
	return rpl::single(
		total()
	) | rpl::then(
		_listChanges.events() | rpl::map([=] { return total(); })
	);
}

void Authorizations::updateTTL(int days) {
	_api.request(_ttlRequestId).cancel();
#if 0 // mtp
	_ttlRequestId = _api.request(MTPaccount_SetAuthorizationTTL(
		MTP_int(days)
#endif
	_ttlRequestId = _api.request(TLsetInactiveSessionTtl(
		tl_int32(days)
	)).done([=] {
		_ttlRequestId = 0;
	}).fail([=] {
		_ttlRequestId = 0;
	}).send();
	_ttlDays = days;
}

rpl::producer<int> Authorizations::ttlDays() const {
	return _ttlDays.value() | rpl::filter(rpl::mappers::_1 != 0);
}

void Authorizations::toggleCallsDisabled(uint64 hash, bool disabled) {
	if (const auto sent = _toggleCallsDisabledRequests.take(hash)) {
		_api.request(*sent).cancel();
	}
#if 0 // mtp
	using Flag = MTPaccount_ChangeAuthorizationSettings::Flag;
	const auto id = _api.request(MTPaccount_ChangeAuthorizationSettings(
		MTP_flags(Flag::f_call_requests_disabled),
		MTP_long(hash),
		MTPBool(), // encrypted_requests_disabled
		MTP_bool(disabled)
#endif
	const auto id = _api.request(TLtoggleSessionCanAcceptCalls(
		tl_int64(hash),
		tl_bool(!disabled)
	)).done([=] {
		_toggleCallsDisabledRequests.remove(hash);
	}).fail([=] {
		_toggleCallsDisabledRequests.remove(hash);
	}).send();
	_toggleCallsDisabledRequests.emplace(hash, id);
	if (!hash) {
		_callsDisabledHere = disabled;
	}
}

bool Authorizations::callsDisabledHere() const {
	return _callsDisabledHere.current();
}

rpl::producer<bool> Authorizations::callsDisabledHereValue() const {
	return _callsDisabledHere.value();
}

rpl::producer<bool> Authorizations::callsDisabledHereChanges() const {
	return _callsDisabledHere.changes();
}

QString Authorizations::ActiveDateString(TimeId active) {
	const auto now = QDateTime::currentDateTime();
	const auto lastTime = base::unixtime::parse(active);
	const auto nowDate = now.date();
	const auto lastDate = lastTime.date();
	return (lastDate == nowDate)
		? QLocale().toString(lastTime.time(), QLocale::ShortFormat)
		: (lastDate.year() == nowDate.year()
			&& lastDate.weekNumber() == nowDate.weekNumber())
		? langDayOfWeek(lastDate)
		: QLocale().toString(lastDate, QLocale::ShortFormat);
}

int Authorizations::total() const {
	return ranges::count_if(
		_list,
		ranges::not_fn(&Entry::incomplete));
}

crl::time Authorizations::lastReceivedTime() {
	return _lastReceived;
}

} // namespace Api
