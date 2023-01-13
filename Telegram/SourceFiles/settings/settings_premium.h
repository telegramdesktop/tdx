/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "settings/settings_type.h"

namespace Tdb {
class TLpremiumFeatures;
} // namespace Tdb

enum class PremiumFeature;

namespace style {
struct RoundButton;
} // namespace style

namespace ChatHelpers {
class Show;
enum class WindowUsage;
} // namespace ChatHelpers

namespace Ui {
class RpWidget;
class RoundButton;
class GradientButton;
class VerticalLayout;
} // namespace Ui

namespace Main {
class Session;
class SessionShow;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

[[nodiscard]] std::optional<Tdb::TLpremiumFeatures> GetPremiumFeaturesSync(
	not_null<::Main::Session*> session,
	const QString &ref);
[[nodiscard]] Fn<void()> CreateStartSubscription(
	not_null<Window::SessionController*> controller,
	const Tdb::TLpremiumFeatures *features);

[[nodiscard]] Type PremiumId();

void ShowPremium(not_null<::Main::Session*> session, const QString &ref);
void ShowPremium(
	not_null<Window::SessionController*> controller,
	const QString &ref);
void ShowGiftPremium(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	int months,
	bool me);
void ShowEmojiStatusPremium(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);

#if 0 // mtp
void StartPremiumPayment(
	not_null<Window::SessionController*> controller,
	const QString &ref);
#endif

[[nodiscard]] QString LookupPremiumRef(PremiumFeature section);

void ShowPremiumPromoToast(
	std::shared_ptr<ChatHelpers::Show> show,
	TextWithEntities textWithLink,
	const QString &ref);
void ShowPremiumPromoToast(
	std::shared_ptr<::Main::SessionShow> show,
	Fn<Window::SessionController*(
		not_null<::Main::Session*>,
		ChatHelpers::WindowUsage)> resolveWindow,
	TextWithEntities textWithLink,
	const QString &ref);

struct SubscribeButtonArgs final {
	Window::SessionController *controller = nullptr;
	not_null<Ui::RpWidget*> parent;
	Fn<QString()> computeRef;
	std::optional<rpl::producer<QString>> text;
	std::optional<QGradientStops> gradientStops;
	Fn<QString()> computeBotUrl; // nullable
	std::shared_ptr<ChatHelpers::Show> show;
	bool showPromo = false;

	required<Fn<void()>> startSubscription;
};


[[nodiscard]] not_null<Ui::RoundButton*> CreateLockedButton(
	not_null<QWidget*> parent,
	rpl::producer<QString> text,
	const style::RoundButton &st,
	rpl::producer<bool> locked);

[[nodiscard]] not_null<Ui::GradientButton*> CreateSubscribeButton(
	SubscribeButtonArgs &&args);

[[nodiscard]] not_null<Ui::GradientButton*> CreateSubscribeButton(
	std::shared_ptr<::Main::SessionShow> show,
	Fn<Window::SessionController*(
		not_null<::Main::Session*>,
		ChatHelpers::WindowUsage)> resolveWindow,
	SubscribeButtonArgs &&args);

[[nodiscard]] std::vector<PremiumFeature> PremiumFeaturesOrder(
	not_null<::Main::Session*> session);

void AddSummaryPremium(
	not_null<Ui::VerticalLayout*> content,
	not_null<Window::SessionController*> controller,
	const QString &ref,
	Fn<void(PremiumFeature)> buttonCallback);

} // namespace Settings

