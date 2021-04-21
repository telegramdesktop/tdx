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

private:
	int errorTop() const override;

	void sendCheckPasswordRequest();
	void setupControls();
	void refreshCode();
#if 0 // #TODO legacy
	void checkForTokenUpdate(const MTPUpdates &updates);
	void checkForTokenUpdate(const MTPUpdate &update);
	void handleTokenResult(const MTPauth_LoginToken &result);
#endif
	void showTokenError(const Tdb::Error &error);
#if 0 // #TODO legacy
	void importTo(MTP::DcId dcId, const QByteArray &token);
#endif
	void showToken(const QByteArray &token);
	void done(const MTPauth_Authorization &authorization);

	void handleUpdate(const Tdb::TLupdate &update);

	rpl::event_stream<QString> _qrLinks;
#if 0 // #TODO legacy
	base::Timer _refreshTimer;
	Tdb::RequestId _requestId = 0;
	bool _forceRefresh = false;
#endif
};

[[nodiscard]] QImage TelegramLogoImage();

} // namespace details
} // namespace Intro
