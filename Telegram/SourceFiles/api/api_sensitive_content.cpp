/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_sensitive_content.h"

#include "apiwrap.h"
#include "base/const_string.h"
#include "tdb/tdb_option.h"
#include "main/main_session.h"
#include "main/main_app_config.h"

namespace Api {
namespace {

constexpr auto kSensitiveContentOption =
	"ignore_sensitive_content_restrictions"_cs;
constexpr auto kRefreshAppConfigTimeout = crl::time(1);

} // namespace

SensitiveContent::SensitiveContent(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance())
, _appConfigReloadTimer([=] { _session->appConfig().refresh(); }) {
}

void SensitiveContent::preload() {
	if (!_loaded) {
		reload();
	}
}

#if 0 // goodToRemove
void SensitiveContent::reload(bool force) {
	if (_loadRequestId) {
		if (force) {
			_loadPending = true;
		}
		return;
	}
	_loaded = true;
	_loadRequestId = _api.request(MTPaccount_GetContentSettings(
	)).done([=](const MTPaccount_ContentSettings &result) {
		_loadRequestId = 0;
		const auto &data = result.data();
		const auto enabled = data.is_sensitive_enabled();
		const auto canChange = data.is_sensitive_can_change();
		const auto changed = (_enabled.current() != enabled)
			|| (_canChange.current() != canChange);
		if (changed) {
			_enabled = enabled;
			_canChange = canChange;
		}
		if (base::take(_appConfigReloadForce) || changed) {
			_appConfigReloadTimer.callOnce(kRefreshAppConfigTimeout);
		}
		if (base::take(_loadPending)) {
			reload();
		}
	}).fail([=] {
		_loadRequestId = 0;
	}).send();
}
#endif

void SensitiveContent::reload(bool force) {
	using namespace Tdb;

	const auto getOption = [&](
			const QString &name,
			not_null<rpl::variable<bool>*> variable) {
		_api.request(TLgetOption(
			tl_string(name)
		)).done([=](const TLoptionValue &result) {
			*variable = Tdb::OptionValue<bool>(result);
		}).fail([](const Error &error) {
		}).send();
	};

	getOption(kSensitiveContentOption.utf8(), &_enabled);
	getOption(u"can_ignore_sensitive_content_restrictions"_q, &_canChange);
}

bool SensitiveContent::enabledCurrent() const {
	return _enabled.current();
}

rpl::producer<bool> SensitiveContent::enabled() const {
	return _enabled.value();
}

rpl::producer<bool> SensitiveContent::canChange() const {
	return _canChange.value();
}

#if 0 // goodToRemove
void SensitiveContent::update(bool enabled) {
	if (!_canChange.current()) {
		return;
	}
	using Flag = MTPaccount_SetContentSettings::Flag;
	_api.request(_saveRequestId).cancel();
	if (const auto load = base::take(_loadRequestId)) {
		_api.request(load).cancel();
		_loadPending = true;
	}
	const auto finish = [=] {
		_saveRequestId = 0;
		if (base::take(_loadPending)) {
			_appConfigReloadForce = true;
			reload(true);
		} else {
			_appConfigReloadTimer.callOnce(kRefreshAppConfigTimeout);
		}
	};
	_saveRequestId = _api.request(MTPaccount_SetContentSettings(
		MTP_flags(enabled ? Flag::f_sensitive_enabled : Flag(0))
	)).done(finish).fail(finish).send();
	_enabled = enabled;
}
#endif

void SensitiveContent::update(bool enabled) {
	if (!_canChange.current()) {
		return;
	}
	using namespace Tdb;
	_api.request(TLsetOption(
		tl_string(kSensitiveContentOption.utf8()),
		tl_optionValueBoolean(tl_bool(enabled))
	)).send();

	_enabled = enabled;

	_appConfigReloadTimer.callOnce(kRefreshAppConfigTimeout);
}

} // namespace Api
