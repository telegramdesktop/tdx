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

namespace Intro {
namespace details {

class QrWidget final : public Step {
public:
	QrWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data);

	void activate() override;
	void finished() override;
	void cancelled() override;
	void submit() override;
	rpl::producer<QString> nextButtonText() const override;

	bool hasBack() const override {
		return true;
	}

	StepType type() const override {
		return StepType::Qr;
	}

	bool applyState(const Tdb::TLauthorizationState &state) override;

private:
	int errorTop() const override;

	void sendCheckPasswordRequest();
	void setupControls();
#if 0 // mtp
	void refreshCode();
	void checkForTokenUpdate(const MTPUpdates &updates);
	void checkForTokenUpdate(const MTPUpdate &update);
	void handleTokenResult(const MTPauth_LoginToken &result);
#endif
	void showTokenError(const Tdb::Error &error);
	void sendRequestCode();
#if 0 // mtp
	void importTo(MTP::DcId dcId, const QByteArray &token);
#endif
	void showToken(const QByteArray &token);
#if 0 // mtp
	void done(const MTPauth_Authorization &authorization);
#endif

	void requestCode();

	rpl::event_stream<QString> _qrLinks;
#if 0 // mtp
	base::Timer _refreshTimer;
#endif
	Tdb::RequestId _requestId = 0;
	bool _forceRefresh = false;
};

[[nodiscard]] QImage TelegramLogoImage();

} // namespace details
} // namespace Intro
