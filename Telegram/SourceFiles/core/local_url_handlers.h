/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Tdb {
class TLinternalLinkType;
} // namespace Tdb

namespace qthelp {
class RegularExpressionMatch;
} // namespace qthelp

namespace Window {
class SessionController;
} // namespace Window

namespace Core {

struct LocalUrlHandler {
	QString expression;
	Fn<bool(
		Window::SessionController *controller,
		const qthelp::RegularExpressionMatch &match,
		const QVariant &context)> handler;
};

#if 0 // mtp
[[nodiscard]] const std::vector<LocalUrlHandler> &LocalUrlHandlers();
#endif
[[nodiscard]] const std::vector<LocalUrlHandler> &InternalUrlHandlers();

[[nodiscard]] QString TryConvertUrlToLocal(QString url);

[[nodiscard]] bool InternalPassportLink(const QString &url);

[[nodiscard]] bool StartUrlRequiresActivate(const QString &url);

struct OpenFromExternalContext {
};
struct StartAttachContext {
	Window::SessionController *controller = nullptr;
	QString botUsername;
	QString openWebAppUrl;

	explicit operator bool() const {
		return !botUsername.isEmpty();
	}
};
bool HandleLocalUrl(
	const Tdb::TLinternalLinkType &link,
	const QVariant &context);

} // namespace Core

Q_DECLARE_METATYPE(Core::OpenFromExternalContext);
Q_DECLARE_METATYPE(Core::StartAttachContext);
