/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_premium_subscription_option.h"

#include "tdb/tdb_tl_scheme.h"
#include "core/local_url_handlers.h"

namespace Api {

[[nodiscard]] Data::PremiumSubscriptionOption CreateSubscriptionOption(
	int months,
	int monthlyAmount,
	int64 amount,
	const QString &currency,
	const QString &botUrl);

template<typename Option>
[[nodiscard]] auto PremiumSubscriptionOptionsFromTL(
		const QVector<Option> &tlOpts) -> Data::PremiumSubscriptionOptions {
	if (tlOpts.isEmpty()) {
		return {};
	}
	auto result = Data::PremiumSubscriptionOptions();
	const auto monthlyAmount = [&] {
		const auto &min = ranges::min_element(
			tlOpts,
			ranges::less(),
			[](const Option &o) { return o.data().vamount().v; }
		)->data();
#if 0 // mtp
		return min.vamount().v / float64(min.vmonths().v);
#endif
		return min.vamount().v / float64(min.vmonth_count().v);
	}();
	using TLGiftCode = Tdb::TLpremiumGiftCodePaymentOption;
	result.reserve(tlOpts.size());
	for (const auto &tlOption : tlOpts) {
		const auto &option = tlOption.data();
#if 0 // mtp
		auto botUrl = QString();
		if constexpr (!std::is_same_v<Option, MTPPremiumGiftCodeOption>) {
			botUrl = qs(option.vbot_url());
		}
		const auto months = option.vmonths().v;
		const auto amount = option.vamount().v;
		const auto currency = qs(option.vcurrency());
#endif
#if 0 // mtp
		const auto botUrl = !lnk
			? QString()
			: (lnk->type() == Tdb::id_internalLinkTypeInvoice)
			? ("https://t.me/$"
				+ lnk->c_internalLinkTypeInvoice().vinvoice_name().v)
			: (lnk->type() == Tdb::id_internalLinkTypeBotStart)
			? ("https://t.me/"
				+ lnk->c_internalLinkTypeBotStart().vbot_username().v
				+ "?start="
				+ lnk->c_internalLinkTypeBotStart().vstart_parameter().v)
			: QString();
#endif
		const auto botUrl = QString();
		const auto months = option.vmonth_count().v;
		const auto amount = option.vamount().v;
		const auto currency = option.vcurrency().v;
		result.push_back(CreateSubscriptionOption(
			months,
			monthlyAmount,
			amount,
			currency,
			botUrl));
		if constexpr (!std::is_same_v<Option, TLGiftCode>) {
			if (const auto lnk = option.vpayment_link()) {
				result.back().startPayment = [=, link = *lnk](QVariant c) {
					Core::HandleLocalUrl(link, c);
				};
			}
		}
	}
	return result;
}

} // namespace Api
