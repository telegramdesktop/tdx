/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "intro/intro_step.h"
#include "core/core_cloud_password.h"
#include "mtproto/sender.h"
#include "base/timer.h"

namespace Ui {
class InputField;
class PasswordInput;
class RoundButton;
class LinkButton;
} // namespace Ui

namespace Intro {
namespace details {

class PasswordCheckWidget final : public Step {
public:
	PasswordCheckWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data);

	void setInnerFocus() override;
	void activate() override;
	void cancelled() override;
	void submit() override;
	rpl::producer<QString> nextButtonText() const override;

	bool hasBack() const override {
		return true;
	}

	StepType type() const override {
		return StepType::Password;
	}

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void toRecover();
	void toPassword();

	int errorTop() const override;

	void showReset();
	void refreshLang();
	void updateControlsGeometry();

#if 0 // mtp
	void pwdSubmitDone(bool recover, const MTPauth_Authorization &result);
	void pwdSubmitFail(const MTP::Error &error);
	void codeSubmitDone(const QString &code, const MTPBool &result);
	void codeSubmitFail(const MTP::Error &error);
	void recoverStartFail(const MTP::Error &error);

	void recoverStarted(const MTPauth_PasswordRecovery &result);
#endif

	void updateDescriptionText();
#if 0 // mtp
	void handleSrpIdInvalid();
	void requestPasswordData();
	void checkPasswordHash();
	void passwordChecked();
	void serverError();
#endif

	void handleAuthorizationState(
		const Tdb::TLauthorizationState &state) override;

	void passwordSubmitFail(const Tdb::Error &error);
	void recoverFail(const Tdb::Error &error);

	Core::CloudPasswordState _passwordState;
#if 0 // mtp
	crl::time _lastSrpIdInvalidTime = 0;
	bytes::vector _passwordHash;
#endif
	QString _emailPattern;

	object_ptr<Ui::PasswordInput> _pwdField;
	object_ptr<Ui::FlatLabel> _pwdHint;
	object_ptr<Ui::InputField> _codeField;
	object_ptr<Ui::LinkButton> _toRecover;
	object_ptr<Ui::LinkButton> _toPassword;
#if 0 // mtp
	mtpRequestId _sentRequest = 0;
#endif

	bool _sentRequest = false;

};

} // namespace details
} // namespace Intro
