/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/countryinput.h"
#include "intro/intro_step.h"
#include "base/timer.h"

namespace Ui {
class PhonePartInput;
class CountryCodeInput;
class RoundButton;
class FlatLabel;
} // namespace Ui

namespace Intro {
namespace details {

class PhoneWidget final : public Step {
public:
	PhoneWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data);
	~PhoneWidget();

	void selectCountry(const QString &country);

	void setInnerFocus() override;
	void activate() override;
	void finished() override;
	void cancelled() override;
	void submit() override;

	bool hasBack() const override {
		return true;
	}

	StepType type() const override {
		return StepType::Phone;
	}

	bool applyState(const Tdb::TLauthorizationState &state) override;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void setupQrLogin();
	void phoneChanged();
	void checkRequest();
	void countryChanged();

#if 0 // mtp
	void phoneSubmitDone(const MTPauth_SentCode &result);
	void phoneSubmitFail(const MTP::Error &error);
#endif
	void sendPhoneRequest(const QString &phone);
	void resendAfterReset(const QString &phone);

	void phoneSetFail(const Tdb::Error &error);

	QString fullNumber() const;
	void stopCheck();

	void showPhoneError(rpl::producer<QString> text);
	void hidePhoneError();

	bool _changed = false;

	object_ptr<CountryInput> _country;
	object_ptr<Ui::CountryCodeInput> _code;
	object_ptr<Ui::PhonePartInput> _phone;

	QString _sentPhone;
#if 0 // mtp
	mtpRequestId _sentRequest = 0;

	base::Timer _checkRequestTimer;
#endif
	bool _resetTried = false;

	bool _sentRequest = false;


};

} // namespace details
} // namespace Intro
