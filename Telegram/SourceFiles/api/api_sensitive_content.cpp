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
#include "main/main_account.h"
#include "main/main_app_config.h"

namespace Api {
namespace {

constexpr auto kSensitiveContentOption =
	"ignore_sensitive_content_restrictions"_cs;
constexpr auto kRefreshAppConfigTimeout = 3 * crl::time(1000);

} // namespace

SensitiveContent::SensitiveContent(not_null<ApiWrap*> api)
: _session(&api->session())
#if 0 // goodToRemove
, _api(&api->instance())
#endif
, _api(&api->sender())
, _appConfigReloadTimer([=] { _session->account().appConfig().refresh(); }) {
}

#if 0 // goodToRemove
void SensitiveContent::reload() {
	if (_requestId) {
		return;
	}
	_requestId = _api.request(MTPaccount_GetContentSettings(
	)).done([=](const MTPaccount_ContentSettings &result) {
		_requestId = 0;
		result.match([&](const MTPDaccount_contentSettings &data) {
			_enabled = data.is_sensitive_enabled();
			_canChange = data.is_sensitive_can_change();
		});
	}).fail([=] {
		_requestId = 0;
	}).send();
}
#endif

void SensitiveContent::reload() {
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
	_api.request(_requestId).cancel();
	_requestId = _api.request(MTPaccount_SetContentSettings(
		MTP_flags(enabled ? Flag::f_sensitive_enabled : Flag(0))
	)).done([=] {
		_requestId = 0;
	}).fail([=] {
		_requestId = 0;
	}).send();
	_enabled = enabled;

	_appConfigReloadTimer.callOnce(kRefreshAppConfigTimeout);
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
