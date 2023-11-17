/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_premium.h"

#include "api/api_premium_option.h"
#include "api/api_text_entities.h"
#include "apiwrap.h"
#include "base/random.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_peer.h"
#include "data/data_peer_values.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/view/history_view_element.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "payments/payments_form.h"
#include "ui/text/format_values.h"

#include "tdb/tdb_tl_scheme.h"
#include "tdb/tdb_sender.h"
#include "boxes/premium_preview_box.h"

namespace Api {
namespace {

using namespace Tdb;

[[nodiscard]] GiftCode Parse(const TLDpremiumGiftCodeInfo &data) {
	return {
		.from = (data.vcreator_id()
			? peerFromSender(*data.vcreator_id())
			: PeerId()),
		.to = data.vuser_id().v ? peerFromUser(data.vuser_id()) : PeerId(),
		.giveawayId = data.vgiveaway_message_id().v,
		.date = data.vcreation_date().v,
		.used = data.vuse_date().v,
		.months = data.vmonth_count().v,
		.giveaway = data.vis_from_giveaway().v,
	};
}

#if 0 // mtp
[[nodiscard]] GiftCode Parse(const MTPDpayments_checkedGiftCode &data) {
	return {
		.from = data.vfrom_id() ? peerFromMTP(*data.vfrom_id()) : PeerId(),
		.to = data.vto_id() ? peerFromUser(*data.vto_id()) : PeerId(),
		.giveawayId = data.vgiveaway_msg_id().value_or_empty(),
		.date = data.vdate().v,
		.used = data.vused_date().value_or_empty(),
		.months = data.vmonths().v,
		.giveaway = data.is_via_giveaway(),
	};
}
#endif

#if 0 // mtp
[[nodiscard]] Data::PremiumSubscriptionOptions GiftCodesFromTL(
		const QVector<MTPPremiumGiftCodeOption> &tlOptions) {
#endif
[[nodiscard]] Data::PremiumSubscriptionOptions GiftCodesFromTL(
		const QVector<Tdb::TLpremiumGiftCodePaymentOption> &tlOptions) {
	auto options = PremiumSubscriptionOptionsFromTL(tlOptions);
	for (auto i = 0; i < options.size(); i++) {
		const auto &tlOption = tlOptions[i].data();
		const auto perUserText = Ui::FillAmountAndCurrency(
#if 0 // mtp
			tlOption.vamount().v / float64(tlOption.vusers().v),
			qs(tlOption.vcurrency()),
#endif
			tlOption.vamount().v / float64(tlOption.vwinner_count().v),
			tlOption.vcurrency().v.toUtf8(),
			false);
		options[i].costPerMonth = perUserText
			+ ' '
			+ QChar(0x00D7)
			+ ' '
			+ QString::number(tlOption.vwinner_count().v);
#if 0 // mtp
			+ QString::number(tlOption.vusers().v);
#endif
	}
	return options;
}

} // namespace

std::optional<PremiumFeature> FeatureFromTL(
		const TLpremiumFeature &feature) {
	const auto result = feature.match([](
			const TLDpremiumFeatureUpgradedStories &) {
		return PremiumFeature::Stories;
	}, [](const TLDpremiumFeatureChatBoost &) {
		return PremiumFeature::kCount;
	}, [](const TLDpremiumFeatureAccentColor &) {
		return PremiumFeature::kCount;
	}, [](const TLDpremiumFeatureIncreasedLimits &) {
		return PremiumFeature::DoubleLimits;
	}, [](const TLDpremiumFeatureIncreasedUploadFileSize &) {
		return PremiumFeature::MoreUpload;
	}, [](const TLDpremiumFeatureImprovedDownloadSpeed &) {
		return PremiumFeature::FasterDownload;
	}, [](const TLDpremiumFeatureVoiceRecognition &) {
		return PremiumFeature::VoiceToText;
	}, [](const TLDpremiumFeatureDisabledAds &) {
		return PremiumFeature::NoAds;
	}, [](const TLDpremiumFeatureUniqueReactions &) {
		return PremiumFeature::InfiniteReactions;
	}, [](const TLDpremiumFeatureUniqueStickers &) {
		return PremiumFeature::Stickers;
	}, [](const TLDpremiumFeatureCustomEmoji &) {
		return PremiumFeature::AnimatedEmoji;
	}, [](const TLDpremiumFeatureAdvancedChatManagement &) {
		return PremiumFeature::AdvancedChatManagement;
	}, [](const TLDpremiumFeatureProfileBadge &) {
		return PremiumFeature::ProfileBadge;
	}, [](const TLDpremiumFeatureEmojiStatus &) {
		return PremiumFeature::EmojiStatus;
	}, [](const TLDpremiumFeatureAnimatedProfilePhoto &) {
		return PremiumFeature::AnimatedUserpics;
	}, [](const TLDpremiumFeatureForumTopicIcon &) {
		return PremiumFeature::kCount;
	}, [](const TLDpremiumFeatureAppIcons &) {
		return PremiumFeature::kCount;
	}, [](const TLDpremiumFeatureRealTimeChatTranslation &) {
		return PremiumFeature::RealTimeTranslation;
	}, [](const TLDpremiumFeatureBackgroundForBoth &) {
		return PremiumFeature::Wallpapers;
	}, [](const TLDpremiumFeatureSavedMessagesTags &) {
		return PremiumFeature::TagsForMessages;
	}, [](const TLDpremiumFeatureMessagePrivacy &) {
		return PremiumFeature::MessagePrivacy;
	}, [](const TLDpremiumFeatureLastSeenTimes &) {
		return PremiumFeature::LastSeen;
	}, [](const TLDpremiumFeatureBusiness &) {
		return PremiumFeature::Business;
	}, [](const TLDpremiumFeatureMessageEffects &) {
		return PremiumFeature::Effects;
	});
	return (result != PremiumFeature::kCount)
		? result
		: std::optional<PremiumFeature>();
}

TLpremiumFeature FeatureToTL(PremiumFeature preview) {
	Expects(preview != PremiumFeature::kCount);

	switch (preview) {
	case PremiumFeature::Stories:
		return tl_premiumFeatureUpgradedStories();
	case PremiumFeature::DoubleLimits:
		return tl_premiumFeatureIncreasedLimits();
	case PremiumFeature::MoreUpload:
		return tl_premiumFeatureIncreasedUploadFileSize();
	case PremiumFeature::FasterDownload:
		return tl_premiumFeatureImprovedDownloadSpeed();
	case PremiumFeature::VoiceToText:
		return tl_premiumFeatureVoiceRecognition();
	case PremiumFeature::NoAds:
		return tl_premiumFeatureDisabledAds();
	case PremiumFeature::InfiniteReactions:
		return tl_premiumFeatureUniqueReactions();
	case PremiumFeature::Stickers:
		return tl_premiumFeatureUniqueStickers();
	case PremiumFeature::AnimatedEmoji:
		return tl_premiumFeatureCustomEmoji();
	case PremiumFeature::AdvancedChatManagement:
		return tl_premiumFeatureAdvancedChatManagement();
	case PremiumFeature::ProfileBadge:
		return tl_premiumFeatureProfileBadge();
	case PremiumFeature::EmojiStatus:
		return tl_premiumFeatureEmojiStatus();
	case PremiumFeature::AnimatedUserpics:
		return tl_premiumFeatureAnimatedProfilePhoto();
	case PremiumFeature::RealTimeTranslation:
		return tl_premiumFeatureRealTimeChatTranslation();
	case PremiumFeature::Wallpapers:
		return tl_premiumFeatureBackgroundForBoth();
	case PremiumFeature::TagsForMessages:
		return tl_premiumFeatureSavedMessagesTags();
	case PremiumFeature::MessagePrivacy:
		return tl_premiumFeatureMessagePrivacy();
	case PremiumFeature::LastSeen:
		return tl_premiumFeatureLastSeenTimes();
	case PremiumFeature::Business:
		return tl_premiumFeatureBusiness();
	case PremiumFeature::Effects:
		return tl_premiumFeatureMessageEffects();
	}
	Unexpected("PremiumFeature value in PreviewToFeature.");
}

Premium::Premium(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance()) {
	crl::on_main(_session, [=] {
		// You can't use _session->user() in the constructor,
		// only queued, because it is not constructed yet.
		Data::AmPremiumValue(
			_session
		) | rpl::start_with_next([=] {
			reload();
			if (_session->premium()) {
				reloadCloudSet();
			}
		}, _session->lifetime());
	});
}

rpl::producer<TextWithEntities> Premium::statusTextValue() const {
	return _statusTextUpdates.events_starting_with_copy(
		_statusText.value_or(TextWithEntities()));
}

auto Premium::videos() const
-> const base::flat_map<PremiumFeature, not_null<DocumentData*>> & {
#if 0 // mtp
-> const base::flat_map<QString, not_null<DocumentData*>> & {
#endif
	return _videos;
}

rpl::producer<> Premium::videosUpdated() const {
	return _videosUpdated.events();
}

auto Premium::stickers() const
-> const std::vector<not_null<DocumentData*>> & {
	return _stickers;
}

rpl::producer<> Premium::stickersUpdated() const {
	return _stickersUpdated.events();
}

auto Premium::cloudSet() const
-> const std::vector<not_null<DocumentData*>> & {
	return _cloudSet;
}

rpl::producer<> Premium::cloudSetUpdated() const {
	return _cloudSetUpdated.events();
}

auto Premium::helloStickers() const
-> const std::vector<not_null<DocumentData*>> & {
	if (_helloStickers.empty()) {
		const_cast<Premium*>(this)->reloadHelloStickers();
	}
	return _helloStickers;
}

rpl::producer<> Premium::helloStickersUpdated() const {
	return _helloStickersUpdated.events();
}

int64 Premium::monthlyAmount() const {
	return _monthlyAmount;
}

QString Premium::monthlyCurrency() const {
	return _monthlyCurrency;
}

void Premium::reload() {
	reloadPromo();
	reloadStickers();
}

void Premium::reloadPromo() {
	if (_promoRequestId) {
		return;
	}
	_promoRequestId = _api.request(TLgetPremiumState(
	)).done([=](const TLDpremiumState &data) {
		_promoRequestId = 0;

		auto list = ranges::views::all(
			data.vpayment_options().v
		) | ranges::views::transform([](
				const TLpremiumStatePaymentOption &option) {
			return option.data().vpayment_option();
		}) | ranges::to<QVector>();

		_subscriptionOptions = PremiumSubscriptionOptionsFromTL(list);
		for (const auto &option : list) {
			if (option.data().vmonth_count().v == 1) {
				_monthlyAmount = option.data().vamount().v;
				_monthlyCurrency = option.data().vcurrency().v;
			}
		}

		auto text = TextWithEntities{
			data.vstate().data().vtext().v,
			EntitiesFromTdb(data.vstate().data().ventities().v),
		};
		_statusText = text;
		_statusTextUpdates.fire(std::move(text));
		auto videos = base::flat_map<
			PremiumFeature,
			not_null<DocumentData*>>();
		videos.reserve(data.vanimations().v.size());
		for (const auto &single : data.vanimations().v) {
			const auto document = _session->data().processDocument(
				single.data().vanimation());
			if ((!document->isVideoFile() && !document->isGifv())
				|| !document->supportsStreaming()) {
				document->forceIsStreamedAnimation();
			}
			const auto type = FeatureFromTL(single.data().vfeature());
			if (type) {
				videos.emplace(*type, document);
			}
		}
		if (_videos != videos) {
			_videos = std::move(videos);
			_videosUpdated.fire({});
		}
	}).fail([=] {
		_promoRequestId = 0;
	}).send();
#if 0 // mtp
	_promoRequestId = _api.request(MTPhelp_GetPremiumPromo(
	)).done([=](const MTPhelp_PremiumPromo &result) {
		_promoRequestId = 0;
		const auto &data = result.data();
		_session->data().processUsers(data.vusers());

		_subscriptionOptions = PremiumSubscriptionOptionsFromTL(
			data.vperiod_options().v);
		for (const auto &option : data.vperiod_options().v) {
			if (option.data().vmonths().v == 1) {
				_monthlyAmount = option.data().vamount().v;
				_monthlyCurrency = qs(option.data().vcurrency());
			}
		}
		auto text = TextWithEntities{
			qs(data.vstatus_text()),
			EntitiesFromMTP(_session, data.vstatus_entities().v),
		};
		_statusText = text;
		_statusTextUpdates.fire(std::move(text));
		auto videos = base::flat_map<QString, not_null<DocumentData*>>();
		const auto count = int(std::min(
			data.vvideo_sections().v.size(),
			data.vvideos().v.size()));
		videos.reserve(count);
		for (auto i = 0; i != count; ++i) {
			const auto document = _session->data().processDocument(
				data.vvideos().v[i]);
			if ((!document->isVideoFile() && !document->isGifv())
				|| !document->supportsStreaming()) {
				document->forceIsStreamedAnimation();
			}
			videos.emplace(
				qs(data.vvideo_sections().v[i]),
				document);
		}
		if (_videos != videos) {
			_videos = std::move(videos);
			_videosUpdated.fire({});
		}
	}).fail([=] {
		_promoRequestId = 0;
	}).send();
#endif
}

void Premium::reloadStickers() {
	if (_stickersRequestId) {
		return;
	}
	_stickersRequestId = _api.request(TLgetPremiumStickerExamples(
	)).done([=](const TLDstickers &result) {
		_stickersRequestId = 0;
		const auto &list = result.vstickers().v;
		const auto owner = &_session->data();
		_stickers.clear();
		_stickers.reserve(list.size());
		for (const auto &sticker : list) {
			const auto document = owner->processDocument(sticker);
			if (document->isPremiumSticker()) {
				_stickers.push_back(document);
			}
		}
		_stickersUpdated.fire({});
	}).fail([=] {
		_stickersRequestId = 0;
	}).send();
#if 0 // mtp
	_stickersRequestId = _api.request(MTPmessages_GetStickers(
		MTP_string("\xe2\xad\x90\xef\xb8\x8f\xe2\xad\x90\xef\xb8\x8f"),
		MTP_long(_stickersHash)
	)).done([=](const MTPmessages_Stickers &result) {
		_stickersRequestId = 0;
		result.match([&](const MTPDmessages_stickersNotModified &) {
		}, [&](const MTPDmessages_stickers &data) {
			_stickersHash = data.vhash().v;
			const auto owner = &_session->data();
			_stickers.clear();
			for (const auto &sticker : data.vstickers().v) {
				const auto document = owner->processDocument(sticker);
				if (document->isPremiumSticker()) {
					_stickers.push_back(document);
				}
			}
			_stickersUpdated.fire({});
		});
	}).fail([=] {
		_stickersRequestId = 0;
	}).send();
#endif
}

void Premium::reloadCloudSet() {
	if (_cloudSetRequestId) {
		return;
	}
#if 0 // mtp
	_cloudSetRequestId = _api.request(MTPmessages_GetStickers(
		MTP_string("\xf0\x9f\x93\x82\xe2\xad\x90\xef\xb8\x8f"),
		MTP_long(_cloudSetHash)
	)).done([=](const MTPmessages_Stickers &result) {
		_cloudSetRequestId = 0;
		result.match([&](const MTPDmessages_stickersNotModified &) {
		}, [&](const MTPDmessages_stickers &data) {
			_cloudSetHash = data.vhash().v;
#endif
	_cloudSetRequestId = _api.request(TLgetPremiumStickers(
		tl_int32(100)
	)).done([=](const TLDstickers &data) {
		_cloudSetRequestId = 0;
		const auto owner = &_session->data();
		_cloudSet.clear();
		for (const auto &sticker : data.vstickers().v) {
			const auto document = owner->processDocument(sticker);
			if (document->isPremiumSticker()) {
				_cloudSet.push_back(document);
			}
		}
		_cloudSetUpdated.fire({});
	}).fail([=] {
		_cloudSetRequestId = 0;
	}).send();
}

void Premium::reloadHelloStickers() {
	if (_helloStickersRequestId) {
		return;
	}
#if 0 // mtp
	_helloStickersRequestId = _api.request(MTPmessages_GetStickers(
		MTP_string("\xf0\x9f\x91\x8b\xe2\xad\x90\xef\xb8\x8f"),
		MTP_long(_helloStickersHash)
	)).done([=](const MTPmessages_Stickers &result) {
		_helloStickersRequestId = 0;
		result.match([&](const MTPDmessages_stickersNotModified &) {
		}, [&](const MTPDmessages_stickers &data) {
			_helloStickersHash = data.vhash().v;
#endif
	_helloStickersRequestId = _api.request(TLgetGreetingStickers(
	)).done([=](const TLstickers &result) {
		_helloStickersRequestId = 0;
		const auto &data = result.data();
			const auto owner = &_session->data();
			_helloStickers.clear();
			for (const auto &sticker : data.vstickers().v) {
				const auto document = owner->processDocument(sticker);
				if (document->sticker()) {
					_helloStickers.push_back(document);
				}
			}
			_helloStickersUpdated.fire({});
#if 0 // mtp
		});
#endif
	}).fail([=] {
		_helloStickersRequestId = 0;
	}).send();
}

void Premium::checkGiftCode(
		const QString &slug,
		Fn<void(GiftCode)> done) {
	if (_giftCodeRequestId) {
		if (_giftCodeSlug == slug) {
			return;
		}
		_api.request(_giftCodeRequestId).cancel();
	}
	_giftCodeSlug = slug;
	_giftCodeRequestId = _api.request(TLcheckPremiumGiftCode(
		tl_string(slug)
	)).done([=](const TLDpremiumGiftCodeInfo &data) {
		_giftCodeRequestId = 0;
		done(updateGiftCode(slug, Parse(data)));
	}).fail([=] {
		_giftCodeRequestId = 0;
		done(updateGiftCode(slug, {}));
	}).send();
#if 0 // mtp
	_giftCodeRequestId = _api.request(MTPpayments_CheckGiftCode(
		MTP_string(slug)
	)).done([=](const MTPpayments_CheckedGiftCode &result) {
		_giftCodeRequestId = 0;

		const auto &data = result.data();
		_session->data().processUsers(data.vusers());
		_session->data().processChats(data.vchats());
		done(updateGiftCode(slug, Parse(data)));
	}).fail([=](const MTP::Error &error) {
		_giftCodeRequestId = 0;

		done(updateGiftCode(slug, {}));
	}).send();
#endif
}

GiftCode Premium::updateGiftCode(
		const QString &slug,
		const GiftCode &code) {
	auto &now = _giftCodes[slug];
	if (now != code) {
		now = code;
		_giftCodeUpdated.fire_copy(slug);
	}
	return code;
}

rpl::producer<GiftCode> Premium::giftCodeValue(const QString &slug) const {
	return _giftCodeUpdated.events_starting_with_copy(
		slug
	) | rpl::filter(rpl::mappers::_1 == slug) | rpl::map([=] {
		const auto i = _giftCodes.find(slug);
		return (i != end(_giftCodes)) ? i->second : GiftCode();
	});
}

void Premium::applyGiftCode(const QString &slug, Fn<void(QString)> done) {
	_api.request(TLapplyPremiumGiftCode(
		tl_string(slug)
	)).done([=] {
		done({});
	}).fail([=](const Error &error) {
		done(error.message);
	}).send();
#if 0 // mtp
	_api.request(MTPpayments_ApplyGiftCode(
		MTP_string(slug)
	)).done([=](const MTPUpdates &result) {
		_session->api().applyUpdates(result);
		done({});
	}).fail([=](const MTP::Error &error) {
		done(error.type());
	}).send();
#endif
}

void Premium::resolveGiveawayInfo(
		not_null<PeerData*> peer,
		MsgId messageId,
		Fn<void(GiveawayInfo)> done) {
	Expects(done != nullptr);

	_giveawayInfoDone = std::move(done);
	if (_giveawayInfoRequestId) {
		if (_giveawayInfoPeer == peer
			&& _giveawayInfoMessageId == messageId) {
			return;
		}
		_api.request(_giveawayInfoRequestId).cancel();
	}
	_giveawayInfoPeer = peer;
	_giveawayInfoMessageId = messageId;
#if 0 // mtp
	_giveawayInfoRequestId = _api.request(MTPpayments_GetGiveawayInfo(
		_giveawayInfoPeer->input,
		MTP_int(_giveawayInfoMessageId.bare)
	)).done([=](const MTPpayments_GiveawayInfo &result) {
		_giveawayInfoRequestId = 0;

		auto info = GiveawayInfo();
		result.match([&](const MTPDpayments_giveawayInfo &data) {
			info.participating = data.is_participating();
			info.state = data.is_preparing_results()
				? GiveawayState::Preparing
				: GiveawayState::Running;
			info.adminChannelId = data.vadmin_disallowed_chat_id()
				? ChannelId(*data.vadmin_disallowed_chat_id())
				: ChannelId();
			info.disallowedCountry = qs(
				data.vdisallowed_country().value_or_empty());
			info.tooEarlyDate
				= data.vjoined_too_early_date().value_or_empty();
			info.startDate = data.vstart_date().v;
		}, [&](const MTPDpayments_giveawayInfoResults &data) {
			info.state = data.is_refunded()
				? GiveawayState::Refunded
				: GiveawayState::Finished;
			info.giftCode = qs(data.vgift_code_slug().value_or_empty());
			info.activatedCount = data.vactivated_count().value_or_empty();
			info.finishDate = data.vfinish_date().v;
			info.startDate = data.vstart_date().v;
			info.credits = data.vstars_prize().value_or_empty();
		});
#endif
	_giveawayInfoRequestId = _api.request(TLgetGiveawayInfo(
		peerToTdbChat(_giveawayInfoPeer->id),
		tl_int53(_giveawayInfoMessageId.bare)
	)).done([=](const TLgiveawayInfo &result) {
		_giveawayInfoRequestId = 0;

		auto info = GiveawayInfo();
		result.match([&](const TLDgiveawayInfoOngoing &data) {
			using AlreadyWasMember
				= TLDgiveawayParticipantStatusAlreadyWasMember;
			using Participating
				= TLDgiveawayParticipantStatusParticipating;
			using Administrator
				= TLDgiveawayParticipantStatusAdministrator;
			using DisallowedCountry
				= TLDgiveawayParticipantStatusDisallowedCountry;

			data.vstatus().match([&](
				const TLDgiveawayParticipantStatusEligible &) {
			}, [&](const Participating &) {
				info.participating = true;
			}, [&](const AlreadyWasMember &data) {
				info.tooEarlyDate = data.vjoined_chat_date().v;
			}, [&](const Administrator &data) {
				info.adminChannelId = peerToChannel(
					peerFromTdbChat(data.vchat_id()));
			}, [&](const DisallowedCountry &data) {
				info.disallowedCountry = data.vuser_country_code().v;
			});
			info.state = data.vis_ended().v
				? GiveawayState::Preparing
				: GiveawayState::Running;
			info.startDate = data.vcreation_date().v;
		}, [&](const TLDgiveawayInfoCompleted &data) {
			info.state = data.vwas_refunded().v
				? GiveawayState::Refunded
				: GiveawayState::Finished;
			info.giftCode = data.vgift_code().v;
			info.activatedCount = data.vactivation_count().v;
			info.finishDate = data.vactual_winners_selection_date().v;
			info.startDate = data.vcreation_date().v;
		});
		_giveawayInfoDone(std::move(info));
	}).fail([=] {
		_giveawayInfoRequestId = 0;
		_giveawayInfoDone({});
	}).send();
}

const Data::PremiumSubscriptionOptions &Premium::subscriptionOptions() const {
	return _subscriptionOptions;
}

rpl::producer<> Premium::somePremiumRequiredResolved() const {
	return _somePremiumRequiredResolved.events();
}

void Premium::resolvePremiumRequired(not_null<UserData*> user) {
	_resolvePremiumRequiredUsers.emplace(user);
	if (!_premiumRequiredRequestScheduled
		&& _resolvePremiumRequestedUsers.empty()) {
		_premiumRequiredRequestScheduled = true;
		crl::on_main(_session, [=] {
			requestPremiumRequiredSlice();
		});
	}
}

void Premium::requestPremiumRequiredSlice() {
	_premiumRequiredRequestScheduled = false;
	if (!_resolvePremiumRequestedUsers.empty()
		|| _resolvePremiumRequiredUsers.empty()) {
		return;
	}
	constexpr auto kPerRequest = 100;
#if 0 // mtp
	auto users = MTP_vector_from_range(_resolvePremiumRequiredUsers
		| ranges::views::transform(&UserData::inputUser));
	if (users.v.size() > kPerRequest) {
		auto shortened = users.v;
		shortened.resize(kPerRequest);
		users = MTP_vector<MTPInputUser>(std::move(shortened));
#endif
	auto users = _resolvePremiumRequiredUsers | ranges::to_vector;
	if (users.size() > kPerRequest) {
		const auto from = begin(_resolvePremiumRequiredUsers);
		_resolvePremiumRequestedUsers = { from, from + kPerRequest };
		_resolvePremiumRequiredUsers.erase(from, from + kPerRequest);
	} else {
		_resolvePremiumRequestedUsers
			= base::take(_resolvePremiumRequiredUsers);
	}
#if 0 // mtp
	const auto finish = [=](const QVector<MTPBool> &list) {
		constexpr auto me = UserDataFlag::MeRequiresPremiumToWrite;
		constexpr auto known = UserDataFlag::RequirePremiumToWriteKnown;
		constexpr auto mask = me | known;

		auto index = 0;
		for (const auto &user : base::take(_resolvePremiumRequestedUsers)) {
			const auto require = (index < list.size())
				&& mtpIsTrue(list[index++]);
			user->setFlags((user->flags() & ~mask)
				| known
				| (require ? me : UserDataFlag()));
		}
		if (!_premiumRequiredRequestScheduled
			&& !_resolvePremiumRequiredUsers.empty()) {
			_premiumRequiredRequestScheduled = true;
			crl::on_main(_session, [=] {
				requestPremiumRequiredSlice();
			});
		}
		_somePremiumRequiredResolved.fire({});
	};
	_session->api().request(
		MTPusers_GetIsPremiumRequiredToContact(std::move(users))
	).done([=](const MTPVector<MTPBool> &result) {
		finish(result.v);
	}).fail([=] {
		finish({});
	}).send();
#endif
	const auto waiting = std::make_shared<int>(
		_resolvePremiumRequestedUsers.size());
	for (const auto &user : _resolvePremiumRequestedUsers) {
		const auto finish = [=](bool require) {
			constexpr auto me = UserDataFlag::MeRequiresPremiumToWrite;
			constexpr auto known = UserDataFlag::RequirePremiumToWriteKnown;
			constexpr auto mask = me | known;

			user->setFlags((user->flags() & ~mask)
				| known
				| (require ? me : UserDataFlag()));
			if (!--*waiting) {
				if (!_premiumRequiredRequestScheduled
					&& !_resolvePremiumRequiredUsers.empty()) {
					_premiumRequiredRequestScheduled = true;
					crl::on_main(_session, [=] {
						requestPremiumRequiredSlice();
					});
				}
				_somePremiumRequiredResolved.fire({});
			}
		};
		user->session().sender().request(TLcanSendMessageToUser(
			tl_int53(peerToUser(user->id).bare),
			tl_bool(false)
		)).done([=](const TLcanSendMessageToUserResult &result) {
			result.match([&](const TLDcanSendMessageToUserResultOk &) {
				finish(false);
			}, [&](const TLDcanSendMessageToUserResultUserIsDeleted &) {
				finish(false);
			}, [&](const TLDcanSendMessageToUserResultUserRestrictsNewChats &) {
				finish(true);
			});
		}).fail([=] {
			finish(false);
		}).send();
	}
}

PremiumGiftCodeOptions::PremiumGiftCodeOptions(not_null<PeerData*> peer)
: _peer(peer)
#if 0 // mtp
, _api(&peer->session().api().instance()) {
#endif
, _api(&peer->session().sender()) {
}

rpl::producer<rpl::no_value, QString> PremiumGiftCodeOptions::request() {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

#if 0 // mtp
		using TLOption = MTPPremiumGiftCodeOption;
		_api.request(MTPpayments_GetPremiumGiftCodeOptions(
			MTP_flags(_peer->isChannel()
				? MTPpayments_GetPremiumGiftCodeOptions::Flag::f_boost_peer
				: MTPpayments_GetPremiumGiftCodeOptions::Flag(0)),
			_peer->input
		)).done([=](const MTPVector<TLOption> &result) {
			auto tlMapOptions = base::flat_map<Amount, QVector<TLOption>>();
			for (const auto &tlOption : result.v) {
				const auto &data = tlOption.data();
				tlMapOptions[data.vusers().v].push_back(tlOption);

				const auto token = Token{ data.vusers().v, data.vmonths().v };
				_stores[token] = Store{
					.amount = data.vamount().v,
					.product = qs(data.vstore_product().value_or_empty()),
					.quantity = data.vstore_quantity().value_or_empty(),
				};
				if (!ranges::contains(_availablePresets, data.vusers().v)) {
					_availablePresets.push_back(data.vusers().v);
				}
			}
			for (const auto &[amount, tlOptions] : tlMapOptions) {
				if (amount == 1 && _optionsForOnePerson.currency.isEmpty()) {
					_optionsForOnePerson.currency = qs(
						tlOptions.front().data().vcurrency());
					for (const auto &option : tlOptions) {
						_optionsForOnePerson.months.push_back(
							option.data().vmonths().v);
						_optionsForOnePerson.totalCosts.push_back(
							option.data().vamount().v);
					}
				}
				_subscriptionOptions[amount] = GiftCodesFromTL(tlOptions);
			}
			consumer.put_done();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
#endif
		using TLOption = Tdb::TLpremiumGiftCodePaymentOption;
		_api.request(Tdb::TLgetPremiumGiftCodePaymentOptions(
			peerToTdbChat(_peer->id)
		)).done([=](const Tdb::TLDpremiumGiftCodePaymentOptions &data) {
			auto tlMapOptions = base::flat_map<Amount, QVector<TLOption>>();
			for (const auto &tlOption : data.voptions().v) {
				const auto &data = tlOption.data();
				const auto userCount = data.vwinner_count().v;
				tlMapOptions[userCount].push_back(tlOption);

				const auto token = Token{ userCount, data.vmonth_count().v };
				_stores[token] = Store{
					.amount = uint64(data.vamount().v),
					.product = data.vstore_product_id().v.toUtf8(),
					.quantity = data.vstore_product_quantity().v,
				};
				if (!ranges::contains(_availablePresets, userCount)) {
					_availablePresets.push_back(userCount);
				}
			}
			for (const auto &[amount, tlOptions] : tlMapOptions) {
				if (amount == 1 && _optionsForOnePerson.currency.isEmpty()) {
					_optionsForOnePerson.currency =
						tlOptions.front().data().vcurrency().v.toUtf8();
					for (const auto &option : tlOptions) {
						_optionsForOnePerson.months.push_back(
							option.data().vmonth_count().v);
						_optionsForOnePerson.totalCosts.push_back(
							option.data().vamount().v);
					}
				}
				_subscriptionOptions[amount] = GiftCodesFromTL(tlOptions);
			}
			consumer.put_done();
		}).fail([=](const Tdb::Error &error) {
			consumer.put_error_copy(error.message);
		}).send();

		return lifetime;
	};
}

rpl::producer<rpl::no_value, QString> PremiumGiftCodeOptions::applyPrepaid(
		const Payments::InvoicePremiumGiftCode &invoice,
		uint64 prepaidId) {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();
		const auto channel = _peer->asChannel();
		if (!channel) {
			return lifetime;
		}

#if 0 // mtp
		_api.request(MTPpayments_LaunchPrepaidGiveaway(
			_peer->input,
			MTP_long(prepaidId),
			invoice.creditsAmount
				? Payments::InvoiceCreditsGiveawayToTL(invoice)
				: Payments::InvoicePremiumGiftCodeGiveawayToTL(invoice)
		)).done([=](const MTPUpdates &result) {
			_peer->session().api().applyUpdates(result);
			consumer.put_done();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
#endif
		_api.request(Tdb::TLlaunchPrepaidGiveaway(
			Tdb::tl_int64(prepaidId),
			Payments::InvoiceGiftCodeGiveawayToTL(invoice),
			tl_int32(invoice.users),
			tl_int53(invoice.creditsAmount.value_or(0))
		)).done([=](const Tdb::TLok &) {
			consumer.put_done();
		}).fail([=](const Tdb::Error &error) {
			consumer.put_error_copy(error.message);
		}).send();

		return lifetime;
	};
}

const std::vector<int> &PremiumGiftCodeOptions::availablePresets() const {
	return _availablePresets;
}

[[nodiscard]] int PremiumGiftCodeOptions::monthsFromPreset(int monthsIndex) {
	Expects(monthsIndex >= 0 && monthsIndex < _availablePresets.size());

	return _optionsForOnePerson.months[monthsIndex];
}

Payments::InvoicePremiumGiftCode PremiumGiftCodeOptions::invoice(
		int users,
		int months) {
	const auto randomId = base::RandomValue<uint64>();
	const auto token = Token{ users, months };
	const auto &store = _stores[token];
	return Payments::InvoicePremiumGiftCode{
		.currency = _optionsForOnePerson.currency,
		.storeProduct = store.product,
		.randomId = randomId,
		.amount = store.amount,
		.storeQuantity = store.quantity,
		.users = token.users,
		.months = token.months,
	};
}

std::vector<GiftOptionData> PremiumGiftCodeOptions::optionsForPeer() const {
	auto result = std::vector<GiftOptionData>();

	if (!_optionsForOnePerson.currency.isEmpty()) {
		const auto count = int(_optionsForOnePerson.months.size());
		result.reserve(count);
		for (auto i = 0; i != count; ++i) {
			Assert(i < _optionsForOnePerson.totalCosts.size());
			result.push_back({
				.cost = _optionsForOnePerson.totalCosts[i],
				.currency = _optionsForOnePerson.currency,
				.months = _optionsForOnePerson.months[i],
			});
		}
	}
	return result;
}

Data::PremiumSubscriptionOptions PremiumGiftCodeOptions::options(int amount) {
	const auto it = _subscriptionOptions.find(amount);
	if (it != end(_subscriptionOptions)) {
		return it->second;
	} else {
#if 0 // mtp
		auto tlOptions = QVector<MTPPremiumGiftCodeOption>();
		for (auto i = 0; i < _optionsForOnePerson.months.size(); i++) {
			tlOptions.push_back(MTP_premiumGiftCodeOption(
				MTP_flags(MTPDpremiumGiftCodeOption::Flags(0)),
				MTP_int(amount),
				MTP_int(_optionsForOnePerson.months[i]),
				MTPstring(),
				MTPint(),
				MTP_string(_optionsForOnePerson.currency),
				MTP_long(_optionsForOnePerson.totalCosts[i] * amount)));
		}
		_subscriptionOptions[amount] = GiftCodesFromTL(tlOptions);
#endif
		auto tlOptions = QVector<Tdb::TLpremiumGiftCodePaymentOption>();
		for (auto i = 0; i < _optionsForOnePerson.months.size(); i++) {
			tlOptions.push_back(Tdb::tl_premiumGiftCodePaymentOption(
				Tdb::tl_string(_optionsForOnePerson.currency),
				Tdb::tl_int53(_optionsForOnePerson.totalCosts[i] * amount),
				Tdb::tl_int32(amount),
				Tdb::tl_int32(_optionsForOnePerson.months[i]),
				Tdb::TLstring(),
				Tdb::TLint32()));
		}
		_subscriptionOptions[amount] = GiftCodesFromTL(tlOptions);
		return _subscriptionOptions[amount];
	}
}

auto PremiumGiftCodeOptions::requestStarGifts()
-> rpl::producer<rpl::no_value, QString> {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

#if 0 // mtp
		_api.request(MTPpayments_GetStarGifts(
			MTP_int(0)
		)).done([=](const MTPpayments_StarGifts &result) {
			result.match([&](const MTPDpayments_starGifts &data) {
				_giftsHash = data.vhash().v;
				const auto &list = data.vgifts().v;
				const auto session = &_peer->session();
				auto gifts = std::vector<StarGift>();
				gifts.reserve(list.size());
				for (const auto &gift : list) {
					if (auto parsed = FromTL(session, gift)) {
						gifts.push_back(std::move(*parsed));
					}
				}
				_gifts = std::move(gifts);
			}, [&](const MTPDpayments_starGiftsNotModified &) {
			});
			consumer.put_done();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
#endif
		_api.request(TLgetAvailableGifts(
		)).done([=](const TLgifts &result) {
			const auto &data = result.data();
			const auto &list = data.vgifts().v;
			const auto session = &_peer->session();
			auto gifts = std::vector<StarGift>();
			gifts.reserve(list.size());
			for (const auto &gift : list) {
				if (auto parsed = FromTL(session, gift)) {
					gifts.push_back(std::move(*parsed));
				}
			}
			_gifts = std::move(gifts);
			consumer.put_done();
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();

		return lifetime;
	};
}

const std::vector<StarGift> &PremiumGiftCodeOptions::starGifts() const {
	return _gifts;
}

int PremiumGiftCodeOptions::giveawayBoostsPerPremium() const {
	constexpr auto kFallbackCount = 4;
	return _peer->session().appConfig().get<int>(
		u"giveaway_boosts_per_premium"_q,
		kFallbackCount);
}

int PremiumGiftCodeOptions::giveawayCountriesMax() const {
	constexpr auto kFallbackCount = 10;
	return _peer->session().appConfig().get<int>(
		u"giveaway_countries_max"_q,
		kFallbackCount);
}

int PremiumGiftCodeOptions::giveawayAddPeersMax() const {
	constexpr auto kFallbackCount = 10;
	return _peer->session().appConfig().get<int>(
		u"giveaway_add_peers_max"_q,
		kFallbackCount);
}

int PremiumGiftCodeOptions::giveawayPeriodMax() const {
	constexpr auto kFallbackCount = 3600 * 24 * 7;
	return _peer->session().appConfig().get<int>(
		u"giveaway_period_max"_q,
		kFallbackCount);
}

bool PremiumGiftCodeOptions::giveawayGiftsPurchaseAvailable() const {
	return _peer->session().appConfig().get<bool>(
		u"giveaway_gifts_purchase_available"_q,
		false);
}

SponsoredToggle::SponsoredToggle(not_null<Main::Session*> session)
: _api(&session->api().instance()) {
}

rpl::producer<bool> SponsoredToggle::toggled() {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

		_api.request(MTPusers_GetFullUser(
			MTP_inputUserSelf()
		)).done([=](const MTPusers_UserFull &result) {
			consumer.put_next_copy(
				result.data().vfull_user().data().is_sponsored_enabled());
		}).fail([=] { consumer.put_next(false); }).send();

		return lifetime;
	};
}

rpl::producer<rpl::no_value, QString> SponsoredToggle::setToggled(bool v) {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

#if 0 // mtp
		_api.request(MTPaccount_ToggleSponsoredMessages(
			MTP_bool(v)
		)).done([=] {
			consumer.put_done();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
#endif
		_api.request(TLtoggleHasSponsoredMessagesEnabled(
			tl_bool(v)
		)).done([=] {
			consumer.put_done();
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();

		return lifetime;
	};
}

RequirePremiumState ResolveRequiresPremiumToWrite(
		not_null<PeerData*> peer,
		History *maybeHistory) {
	const auto user = peer->asUser();
	if (!user
		|| !user->someRequirePremiumToWrite()
		|| user->session().premium()) {
		return RequirePremiumState::No;
	} else if (user->requirePremiumToWriteKnown()) {
		return user->meRequiresPremiumToWrite()
			? RequirePremiumState::Yes
			: RequirePremiumState::No;
	} else if (user->flags() & UserDataFlag::MutualContact) {
		return RequirePremiumState::No;
	} else if (!maybeHistory) {
		return RequirePremiumState::Unknown;
	}

	const auto update = [&](bool require) {
		using Flag = UserDataFlag;
		constexpr auto known = Flag::RequirePremiumToWriteKnown;
		constexpr auto me = Flag::MeRequiresPremiumToWrite;
		user->setFlags((user->flags() & ~me)
			| known
			| (require ? me : Flag()));
	};
	// We allow this potentially-heavy loop because in case we've opened
	// the chat and have a lot of messages `requires_premium` will be known.
	for (const auto &block : maybeHistory->blocks) {
		for (const auto &view : block->messages) {
			const auto item = view->data();
			if (!item->out() && !item->isService()) {
				update(false);
				return RequirePremiumState::No;
			}
		}
	}
	if (user->isContact() // Here we know, that we're not in his contacts.
		&& maybeHistory->loadedAtTop() // And no incoming messages.
		&& maybeHistory->loadedAtBottom()) {
		update(true);
	}
	return RequirePremiumState::Unknown;
}

rpl::producer<DocumentData*> RandomHelloStickerValue(
		not_null<Main::Session*> session) {
	const auto premium = &session->api().premium();
	const auto random = [=] {
		const auto &v = premium->helloStickers();
		Assert(!v.empty());
		return v[base::RandomIndex(v.size())].get();
	};
	const auto &v = premium->helloStickers();
	if (!v.empty()) {
		return rpl::single(random());
	}
	return rpl::single<DocumentData*>(
		nullptr
	) | rpl::then(premium->helloStickersUpdated(
	) | rpl::filter([=] {
		return !premium->helloStickers().empty();
	}) | rpl::take(1) | rpl::map(random));
}

#if 0 // mtp
std::optional<StarGift> FromTL(
		not_null<Main::Session*> session,
		const MTPstarGift &gift) {
	const auto &data = gift.data();
	const auto document = session->data().processDocument(
		data.vsticker());
	const auto remaining = data.vavailability_remains();
	const auto total = data.vavailability_total();
	if (!document->sticker()) {
		return {};
	}
	return StarGift{
		.id = uint64(data.vid().v),
		.stars = int64(data.vstars().v),
		.convertStars = int64(data.vconvert_stars().v),
		.document = document,
		.limitedLeft = remaining.value_or_empty(),
		.limitedCount = total.value_or_empty(),
	};
}

std::optional<UserStarGift> FromTL(
		not_null<UserData*> to,
		const MTPuserStarGift &gift) {
	const auto session = &to->session();
	const auto &data = gift.data();
	auto parsed = FromTL(session, data.vgift());
	if (!parsed) {
		return {};
	}
	return UserStarGift{
		.gift = std::move(*parsed),
		.message = (data.vmessage()
			? TextWithEntities{
				.text = qs(data.vmessage()->data().vtext()),
				.entities = Api::EntitiesFromMTP(
					session,
					data.vmessage()->data().ventities().v),
			}
			: TextWithEntities()),
		.convertStars = int64(data.vconvert_stars().value_or_empty()),
		.fromId = (data.vfrom_id()
			? peerFromUser(data.vfrom_id()->v)
			: PeerId()),
		.messageId = data.vmsg_id().value_or_empty(),
		.date = data.vdate().v,
		.anonymous = data.is_name_hidden(),
		.hidden = data.is_unsaved(),
		.mine = to->isSelf(),
	};
}
#endif

std::optional<StarGift> FromTL(
		not_null<Main::Session*> session,
		const TLgift &gift) {
	const auto &data = gift.data();
	const auto document = session->data().processDocument(
		data.vsticker());
	if (!document->sticker()) {
		return {};
	}
	return StarGift{
		.id = uint64(data.vid().v),
		.stars = int64(data.vstar_count().v),
		.convertStars = int64(data.vdefault_sell_star_count().v),
		.document = document,
		.limitedLeft = data.vremaining_count().v,
		.limitedCount = data.vtotal_count().v,
	};
}

std::optional<UserStarGift> FromTL(
		not_null<UserData*> to,
		const TLuserGift &gift) {
	const auto session = &to->session();
	const auto &data = gift.data();
	auto parsed = FromTL(session, data.vgift());
	if (!parsed) {
		return {};
	}
	return UserStarGift{
		.gift = std::move(*parsed),
		.message = Api::FormattedTextFromTdb(data.vtext()),
		.convertStars = data.vsell_star_count().v,
		.fromId = peerFromUser(data.vsender_user_id()),
		.messageId = data.vmessage_id().v,
		.date = data.vdate().v,
		.anonymous = data.vis_private().v,
		.hidden = !data.vis_saved().v,
		.mine = to->isSelf(),
	};
}

} // namespace Api
