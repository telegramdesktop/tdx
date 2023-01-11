/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_premium.h"

#include "api/api_premium_option.h"
#include "api/api_text_entities.h"
#include "main/main_session.h"
#include "data/data_peer_values.h"
#include "data/data_document.h"
#include "data/data_session.h"
#include "data/data_peer.h"
#include "apiwrap.h"

#include "tdb/tdb_tl_scheme.h"
#include "tdb/tdb_sender.h"
#include "boxes/premium_preview_box.h"

namespace Api {
namespace {

using namespace Tdb;

} // namespace

std::optional<PremiumPreview> PreviewFromFeature(
		const TLpremiumFeature &feature) {
	const auto result = feature.match([](
			const TLDpremiumFeatureUpgradedStories &) {
		return PremiumPreview::Stories;
	}, [](const TLDpremiumFeatureChatBoost &) {
		return PremiumPreview::kCount;
	}, [](const TLDpremiumFeatureIncreasedLimits &) {
		return PremiumPreview::DoubleLimits;
	}, [](const TLDpremiumFeatureIncreasedUploadFileSize &) {
		return PremiumPreview::MoreUpload;
	}, [](const TLDpremiumFeatureImprovedDownloadSpeed &) {
		return PremiumPreview::FasterDownload;
	}, [](const TLDpremiumFeatureVoiceRecognition &) {
		return PremiumPreview::VoiceToText;
	}, [](const TLDpremiumFeatureDisabledAds &) {
		return PremiumPreview::NoAds;
	}, [](const TLDpremiumFeatureUniqueReactions &) {
		return PremiumPreview::InfiniteReactions;
	}, [](const TLDpremiumFeatureUniqueStickers &) {
		return PremiumPreview::Stickers;
	}, [](const TLDpremiumFeatureCustomEmoji &) {
		return PremiumPreview::AnimatedEmoji;
	}, [](const TLDpremiumFeatureAdvancedChatManagement &) {
		return PremiumPreview::AdvancedChatManagement;
	}, [](const TLDpremiumFeatureProfileBadge &) {
		return PremiumPreview::ProfileBadge;
	}, [](const TLDpremiumFeatureEmojiStatus &) {
		return PremiumPreview::EmojiStatus;
	}, [](const TLDpremiumFeatureAnimatedProfilePhoto &) {
		return PremiumPreview::AnimatedUserpics;
	}, [](const TLDpremiumFeatureForumTopicIcon &) {
		return PremiumPreview::kCount;
	}, [](const TLDpremiumFeatureAppIcons &) {
		return PremiumPreview::kCount;
	});
	return (result != PremiumPreview::kCount)
		? result
		: std::optional<PremiumPreview>();
}

TLpremiumFeature PreviewToFeature(PremiumPreview preview) {
	Expects(preview != PremiumPreview::kCount);

	switch (preview) {
	case PremiumPreview::Stories:
		return tl_premiumFeatureUpgradedStories();
	case PremiumPreview::DoubleLimits:
		return tl_premiumFeatureIncreasedLimits();
	case PremiumPreview::MoreUpload:
		return tl_premiumFeatureIncreasedUploadFileSize();
	case PremiumPreview::FasterDownload:
		return tl_premiumFeatureImprovedDownloadSpeed();
	case PremiumPreview::VoiceToText:
		return tl_premiumFeatureVoiceRecognition();
	case PremiumPreview::NoAds:
		return tl_premiumFeatureDisabledAds();
	case PremiumPreview::InfiniteReactions:
		return tl_premiumFeatureUniqueReactions();
	case PremiumPreview::Stickers:
		return tl_premiumFeatureUniqueStickers();
	case PremiumPreview::AnimatedEmoji:
		return tl_premiumFeatureCustomEmoji();
	case PremiumPreview::AdvancedChatManagement:
		return tl_premiumFeatureAdvancedChatManagement();
	case PremiumPreview::ProfileBadge:
		return tl_premiumFeatureProfileBadge();
	case PremiumPreview::EmojiStatus:
		return tl_premiumFeatureEmojiStatus();
	case PremiumPreview::AnimatedUserpics:
		return tl_premiumFeatureAnimatedProfilePhoto();
	}
	Unexpected("PremiumPreview value in PreviewToFeature.");
}

Premium::Premium(not_null<ApiWrap*> api)
: _session(&api->session())
#if 0 // mtp
, _api(&api->instance()) {
#endif
, _api(&_session->sender()) {
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
-> const base::flat_map<PremiumPreview, not_null<DocumentData*>> & {
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

		_subscriptionOptions = SubscriptionOptionsFromTL(
			data.vpayment_options().v);
		for (const auto &option : data.vpayment_options().v) {
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
			PremiumPreview,
			not_null<DocumentData*>>();
		videos.reserve(data.vanimations().v.size());
		for (const auto &single : data.vanimations().v) {
			const auto document = _session->data().processDocument(
				single.data().vanimation());
			if ((!document->isVideoFile() && !document->isGifv())
				|| !document->supportsStreaming()) {
				document->forceIsStreamedAnimation();
			}
			const auto type = PreviewFromFeature(single.data().vfeature());
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

		_subscriptionOptions = SubscriptionOptionsFromTL(
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

const Data::SubscriptionOptions &Premium::subscriptionOptions() const {
	return _subscriptionOptions;
}

} // namespace Api
