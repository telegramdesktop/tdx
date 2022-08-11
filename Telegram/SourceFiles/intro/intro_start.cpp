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

#include "intro/intro_widget.h"
#include "tdb/tdb_tl_scheme.h"

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
}

void StartWidget::submit() {
#if 0 // mtp
	account().destroyStaleAuthorizationKeys();
	goNext<QrWidget>();
#endif

	api().request(
		TLgetAuthorizationState()
	).done([=](const TLauthorizationState &result) {
		jumpByState(result);
	}).send();
}

rpl::producer<QString> StartWidget::nextButtonText() const {
	return tr::lng_start_msgs();
}

bool StartWidget::applyState(const TLauthorizationState &state) {
	const auto type = state.type();
	return (type == id_authorizationStateWaitPhoneNumber)
		|| (type == id_authorizationStateWaitOtherDeviceConfirmation)
		|| (type == id_authorizationStateLoggingOut)
		|| (type == id_authorizationStateClosing)
		|| (type == id_authorizationStateClosed)
		|| (type == id_authorizationStateWaitCode
			&& getData()->madeInitialJumpToStep);
}

} // namespace details
} // namespace Intro
