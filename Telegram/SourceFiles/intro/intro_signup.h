/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "intro/intro_step.h"

namespace Ui {
class RoundButton;
class InputField;
class UserpicButton;
} // namespace Ui

namespace Intro {
namespace details {

class SignupWidget final : public Step {
public:
	SignupWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data);

	void finishInit() override;
	void setInnerFocus() override;
	void activate() override;
	void cancelled() override;
	void submit() override;
	rpl::producer<QString> nextButtonText() const override;

	StepType type() const override {
		return StepType::SignUp;
	}

	bool applyState(const Tdb::TLauthorizationState &state) override;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void refreshLang();
	void updateControlsGeometry();

#if 0 // mtp
	void nameSubmitDone(const MTPauth_Authorization &result);
	void nameSubmitFail(const MTP::Error &error);
#endif

	void registerUserFail(const Tdb::Error &error);

	object_ptr<Ui::UserpicButton> _photo;
	object_ptr<Ui::InputField> _first;
	object_ptr<Ui::InputField> _last;
	QString _firstName, _lastName;
#if 0 // mtp
	mtpRequestId _sentRequest = 0;
#endif

	bool _invertOrder = false;

	bool _termsAccepted = false;
	bool _sentRequest = false;

};

} // namespace details
} // namespace Intro
