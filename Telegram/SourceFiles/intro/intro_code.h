/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "intro/intro_step.h"
#include "intro/intro_widget.h"
#include "ui/widgets/fields/masked_input_field.h"
#include "base/timer.h"

namespace Ui {
class RoundButton;
class LinkButton;
class FlatLabel;
class CodeInput;
} // namespace Ui

namespace Intro {
namespace details {

enum class CallStatus;

class CodeWidget final : public Step {
public:
	CodeWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data);

	bool hasBack() const override {
		return true;
	}
	void setInnerFocus() override;
	void activate() override;
	void finished() override;
	void cancelled() override;
	void submit() override;
	rpl::producer<QString> nextButtonText() const override;
	rpl::producer<const style::RoundButton*> nextButtonStyle() const override;

	void updateDescText();

	StepType type() const override {
		return StepType::Code;
	}

	bool applyState(const Tdb::TLauthorizationState &state) override;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void noTelegramCode();
	void sendCall();
#if 0 // mtp
	void checkRequest();
#endif

	int errorTop() const override;

	void updateCallText();
	void refreshLang();
	void updateControlsGeometry();

#if 0 // mtp
	void codeSubmitDone(const MTPauth_Authorization &result);
	void codeSubmitFail(const MTP::Error &error);
#endif

	void showCodeError(rpl::producer<QString> text);
#if 0 // mtp
	void callDone(const MTPauth_SentCode &result);
	void gotPassword(const MTPaccount_Password &result);

	void noTelegramCodeDone(const MTPauth_SentCode &result);
	void noTelegramCodeFail(const MTP::Error &result);

	void submitCode(const QString &text);

	void stopCheck();
#endif

	void checkCodeFail(const Tdb::Error &error);

	object_ptr<Ui::LinkButton> _noTelegramCode;
#if 0 // mtp
	mtpRequestId _noTelegramCodeRequestId = 0;
#endif

	object_ptr<Ui::CodeInput> _code;
	QString _sentCode;
#if 0 // mtp
	mtpRequestId _sentRequest = 0;
#endif

	rpl::variable<bool> _isFragment = false;

	base::Timer _callTimer;
	CallStatus _callStatus = CallStatus();
	int _callTimeout;
#if 0 // mtp
	mtpRequestId _callRequestId = 0;
#endif
	object_ptr<Ui::FlatLabel> _callLabel;

#if 0 // mtp
	base::Timer _checkRequestTimer;
#endif

	bool _noTelegramCodeSent = false;
	bool _sentRequest = false;

};

} // namespace details
} // namespace Intro
