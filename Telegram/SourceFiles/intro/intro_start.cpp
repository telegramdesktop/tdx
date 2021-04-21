/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "intro/intro_start.h"

#include "lang/lang_keys.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "main/main_account.h"
#include "main/main_app_config.h"

namespace Intro {
namespace details {
namespace {

using namespace Tdb;

} // namespace

StartWidget::StartWidget(
	QWidget *parent,
	not_null<Main::Account*> account,
	not_null<Data*> data)
: Step(parent, account, data, true) {
	setMouseTracking(true);
	setTitleText(rpl::single(u"Telegram Desktop"_q));
	setDescriptionText(tr::lng_intro_about());
	show();

	api().request(
		TLgetAuthorizationState()
	).done([=](const TLauthorizationState &result) {
		handleAuthorizationState(result);
	}).send();
}

void StartWidget::submit() {
#if 0 // #TODO legacy
	account().destroyStaleAuthorizationKeys();
	goNext<QrWidget>();
#endif

	api().request(
		TLgetAuthorizationState()
	).done([=](const TLauthorizationState &result) {
		if (result.type() == id_authorizationStateWaitPhoneNumber) {
			go(StepType::Qr);
		} else {
			Step::handleAuthorizationState(result);
		}
	}).send();
}

rpl::producer<QString> StartWidget::nextButtonText() const {
	return tr::lng_start_msgs();
}

void StartWidget::handleAuthorizationState(
		const TLauthorizationState &state) {
	return state.match([&](const auto &data) {
		using Data = decltype(data);
		if constexpr (TLDauthorizationStateReady::Is<Data>()
			|| TLDauthorizationStateWaitCode::Is<Data>()
			|| TLDauthorizationStateWaitOtherDeviceConfirmation::Is<Data>()
			|| TLDauthorizationStateWaitRegistration::Is<Data>()
			|| TLDauthorizationStateWaitPassword::Is<Data>()) {
			Step::handleAuthorizationState(state);
		}
	});
}

} // namespace details
} // namespace Intro
