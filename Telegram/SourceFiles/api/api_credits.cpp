/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_credits.h"

#include "api/api_statistics_data_deserialize.h"
#include "api/api_updates.h"
#include "apiwrap.h"
#include "base/unixtime.h"
#include "data/components/credits.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_app_config.h"
#include "main/main_session.h"

#include "tdb/tdb_tl_scheme.h"
#include "api/api_text_entities.h"
#include "data/data_credits.h"

namespace Api {
namespace {

using namespace Tdb;

constexpr auto kTransactionsLimit = 100;
constexpr auto kTransactionsPerStatus = 3;
constexpr auto kTransactionsPerPage = 50;

#if 0 // mtp
[[nodiscard]] Data::CreditsHistoryEntry HistoryFromTL(
		const MTPStarsTransaction &tl,
		not_null<PeerData*> peer) {
	using HistoryPeerTL = MTPDstarsTransactionPeer;
	using namespace Data;
	const auto owner = &peer->owner();
	const auto photo = tl.data().vphoto()
		? owner->photoFromWeb(*tl.data().vphoto(), ImageLocation())
		: nullptr;
	auto extended = std::vector<CreditsHistoryMedia>();
	if (const auto list = tl.data().vextended_media()) {
		extended.reserve(list->v.size());
		for (const auto &media : list->v) {
			media.match([&](const MTPDmessageMediaPhoto &photo) {
				if (const auto inner = photo.vphoto()) {
					const auto photo = owner->processPhoto(*inner);
					if (!photo->isNull()) {
						extended.push_back(CreditsHistoryMedia{
							.type = CreditsHistoryMediaType::Photo,
							.id = photo->id,
						});
					}
				}
			}, [&](const MTPDmessageMediaDocument &document) {
				if (const auto inner = document.vdocument()) {
					const auto document = owner->processDocument(*inner);
					if (document->isAnimation()
						|| document->isVideoFile()
						|| document->isGifv()) {
						extended.push_back(CreditsHistoryMedia{
							.type = CreditsHistoryMediaType::Video,
							.id = document->id,
						});
					}
				}
			}, [&](const auto &) {});
		}
	}
	const auto barePeerId = tl.data().vpeer().match([](
			const HistoryPeerTL &p) {
		return peerFromMTP(p.vpeer());
	}, [](const auto &) {
		return PeerId(0);
	}).value;
	const auto stargift = tl.data().vstargift();
	const auto incoming = (int64(tl.data().vstars().v) >= 0);
	return Data::CreditsHistoryEntry{
		.id = qs(tl.data().vid()),
		.title = qs(tl.data().vtitle().value_or_empty()),
		.description = { qs(tl.data().vdescription().value_or_empty()) },
		.date = base::unixtime::parse(tl.data().vdate().v),
		.photoId = photo ? photo->id : 0,
		.extended = std::move(extended),
		.credits = tl.data().vstars().v,
		.bareMsgId = uint64(tl.data().vmsg_id().value_or_empty()),
		.barePeerId = barePeerId,
		.bareGiveawayMsgId = uint64(
			tl.data().vgiveaway_post_id().value_or_empty()),
		.bareGiftStickerId = (stargift
			? owner->processDocument(stargift->data().vsticker())->id
			: 0),
		.peerType = tl.data().vpeer().match([](const HistoryPeerTL &) {
			return Data::CreditsHistoryEntry::PeerType::Peer;
		}, [](const MTPDstarsTransactionPeerPlayMarket &) {
			return Data::CreditsHistoryEntry::PeerType::PlayMarket;
		}, [](const MTPDstarsTransactionPeerFragment &) {
			return Data::CreditsHistoryEntry::PeerType::Fragment;
		}, [](const MTPDstarsTransactionPeerAppStore &) {
			return Data::CreditsHistoryEntry::PeerType::AppStore;
		}, [](const MTPDstarsTransactionPeerUnsupported &) {
			return Data::CreditsHistoryEntry::PeerType::Unsupported;
		}, [](const MTPDstarsTransactionPeerPremiumBot &) {
			return Data::CreditsHistoryEntry::PeerType::PremiumBot;
		}, [](const MTPDstarsTransactionPeerAds &) {
			return Data::CreditsHistoryEntry::PeerType::Ads;
		}),
		.subscriptionUntil = tl.data().vsubscription_period()
			? base::unixtime::parse(base::unixtime::now()
				+ tl.data().vsubscription_period()->v)
			: QDateTime(),
		.successDate = tl.data().vtransaction_date()
			? base::unixtime::parse(tl.data().vtransaction_date()->v)
			: QDateTime(),
		.successLink = qs(tl.data().vtransaction_url().value_or_empty()),
		.convertStars = int(stargift
			? stargift->data().vconvert_stars().v
			: 0),
		.converted = stargift && incoming,
		.reaction = tl.data().is_reaction(),
		.refunded = tl.data().is_refund(),
		.pending = tl.data().is_pending(),
		.failed = tl.data().is_failed(),
		.in = incoming,
		.gift = tl.data().is_gift() || stargift.has_value(),
	};
}

[[nodiscard]] Data::SubscriptionEntry SubscriptionFromTL(
		const MTPStarsSubscription &tl) {
	return Data::SubscriptionEntry{
		.id = qs(tl.data().vid()),
		.inviteHash = qs(tl.data().vchat_invite_hash().value_or_empty()),
		.until = base::unixtime::parse(tl.data().vuntil_date().v),
		.subscription = Data::PeerSubscription{
			.credits = tl.data().vpricing().data().vamount().v,
			.period = tl.data().vpricing().data().vperiod().v,
		},
		.barePeerId = peerFromMTP(tl.data().vpeer()).value,
		.cancelled = tl.data().is_canceled(),
		.expired = (base::unixtime::now() > tl.data().vuntil_date().v),
		.canRefulfill = tl.data().is_can_refulfill(),
	};
}

[[nodiscard]] Data::CreditsStatusSlice StatusFromTL(
		const MTPpayments_StarsStatus &status,
		not_null<PeerData*> peer) {
	const auto &data = status.data();
	peer->owner().processUsers(data.vusers());
	peer->owner().processChats(data.vchats());
	auto entries = std::vector<Data::CreditsHistoryEntry>();
	if (const auto history = data.vhistory()) {
		entries.reserve(history->v.size());
		for (const auto &tl : history->v) {
			entries.push_back(HistoryFromTL(tl, peer));
		}
	}
	auto subscriptions = std::vector<Data::SubscriptionEntry>();
	if (const auto history = data.vsubscriptions()) {
		subscriptions.reserve(history->v.size());
		for (const auto &tl : history->v) {
			subscriptions.push_back(SubscriptionFromTL(tl));
		}
	}
	return Data::CreditsStatusSlice{
		.list = std::move(entries),
		.subscriptions = std::move(subscriptions),
		.balance = status.data().vbalance().v,
		.subscriptionsMissingBalance
			= status.data().vsubscriptions_missing_balance().value_or_empty(),
		.allLoaded = !status.data().vnext_offset().has_value(),
		.token = qs(status.data().vnext_offset().value_or_empty()),
		.tokenSubscriptions = qs(
			status.data().vsubscriptions_next_offset().value_or_empty()),
	};
}
#endif

[[nodiscard]] Data::CreditsHistoryEntry HistoryFromTL(
		const TLstarTransaction &tl,
		not_null<PeerData*> peer) {
	const auto &data = tl.data();
	auto result = Data::CreditsHistoryEntry{
		.id = data.vid().v,
		.date = base::unixtime::parse(data.vdate().v),
		.credits = uint64(data.vstar_count().v),
		.refunded = data.vis_refund().v,
		.in = (int64(data.vstar_count().v) >= 0),
	};
	const auto fillPaidMedia = [&](const TLvector<TLpaidMedia> &media) {
		const auto list = media.v;
		if (list.empty()) {
			return;
		}
		using namespace Data;
		const auto owner = &peer->owner();
		result.extended.reserve(list.size());
		for (const auto &media : list) {
			media.match([&](const TLDpaidMediaPhoto &data) {
				const auto photo = owner->processPhoto(data.vphoto());
				if (!photo->isNull()) {
					result.extended.push_back(Data::CreditsHistoryMedia{
						.type = Data::CreditsHistoryMediaType::Photo,
						.id = photo->id,
					});
				}
			}, [&](const TLDpaidMediaVideo &data) {
				const auto document = owner->processDocument(
					data.vvideo());
				if (document->isAnimation()
					|| document->isVideoFile()
					|| document->isGifv()) {
					result.extended.push_back(Data::CreditsHistoryMedia{
						.type = Data::CreditsHistoryMediaType::Video,
						.id = document->id,
					});
				}
			}, [&](const auto &) {});
		}
	};

	using Type = Data::CreditsHistoryEntry::PeerType;
	data.vpartner().match([&](const TLDstarTransactionPartnerTelegram &) {
		result.peerType = Type::PremiumBot;
	}, [&](const TLDstarTransactionPartnerAppStore &) {
		result.peerType = Type::AppStore;
	}, [&](const TLDstarTransactionPartnerGooglePlay &) {
		result.peerType = Type::PlayMarket;
	}, [&](const TLDstarTransactionPartnerFragment &data) {
		result.peerType = Type::Fragment;
		if (const auto state = data.vwithdrawal_state()) {
			state->match([&](const TLDrevenueWithdrawalStateFailed &) {
				result.failed = true;
			}, [&](const TLDrevenueWithdrawalStatePending &) {
				result.pending = true;
			}, [&](const TLDrevenueWithdrawalStateSucceeded &data) {
				result.successDate = base::unixtime::parse(data.vdate().v);
				result.successLink = data.vurl().v;
			});
		}
	}, [&](const TLDstarTransactionPartnerTelegramAds &data) {
		result.peerType = Type::Ads;
	}, [&](const TLDstarTransactionPartnerBot &data) {
		data.vpurpose().match([&](
				const TLDbotTransactionPurposePaidMedia &data) {
			fillPaidMedia(data.vmedia());
		}, [&](const TLDbotTransactionPurposeInvoicePayment &data) {
			if (const auto product = data.vproduct_info()) {
				const auto &data = product->data();
				result.title = data.vtitle().v;
				result.description = Api::FormattedTextFromTdb(
					data.vdescription());
				if (const auto photo = data.vphoto()) {
					result.photoId = peer->owner().processPhoto(*photo)->id;
				}
			}
		});
		result.barePeerId = peerFromUser(data.vuser_id().v).value;
		result.peerType = Type::Peer;
	}, [&](const TLDstarTransactionPartnerBusiness &data) {
		fillPaidMedia(data.vmedia());
		result.barePeerId = peerFromUser(data.vuser_id().v).value;
		result.peerType = Type::Peer;
	}, [&](const TLDstarTransactionPartnerChat &data) {
		using namespace Data;
		result.barePeerId = peerFromTdbChat(data.vchat_id()).value;
		data.vpurpose().match([&](
			const TLDchatTransactionPurposeJoin &data) {
			result.subscriptionUntil = base::unixtime::parse(
				base::unixtime::now() + data.vperiod().v);
		}, [&](const TLDchatTransactionPurposePaidMedia &data) {
			result.bareMsgId = data.vmessage_id().v;
			fillPaidMedia(data.vmedia());
		}, [&](const TLDchatTransactionPurposeReaction &data) {
			result.bareMsgId = data.vmessage_id().v;
			result.reaction = true;
		}, [&](const TLDchatTransactionPurposeGiveaway &data) {
			result.bareGiveawayMsgId = uint64(data.vgiveaway_message_id().v);
		});
		result.peerType = Type::Peer;
	}, [&](const TLDstarTransactionPartnerUser &data) {
		using namespace Data;
		result.barePeerId = peerFromUser(data.vuser_id()).value;
		data.vpurpose().match([&](
				const TLDuserTransactionPurposeGiftSell &data) {
			const auto &gift = data.vgift().data();
			result.convertStars = gift.vdefault_sell_star_count().v;
			result.converted = true;
		}, [&](const TLDuserTransactionPurposeGiftSend &data) {
			const auto &gift = data.vgift().data();
			result.convertStars = gift.vdefault_sell_star_count().v;
		}, [&](const TLDuserTransactionPurposeGiftedStars &data) {
			result.bareGiftStickerId = data.vsticker()
				? peer->owner().processDocument(*data.vsticker())->id
				: 0;
		});
		result.gift = true;
		result.peerType = Type::Peer;
	}, [&](const TLDstarTransactionPartnerUnsupported &) {
		result.peerType = Type::Unsupported;
	});
	return result;
}

[[nodiscard]] Data::SubscriptionEntry SubscriptionFromTL(
		const TLstarSubscription &tl) {
	const auto &data = tl.data();
	return Data::SubscriptionEntry{
		.id = data.vid().v,
		.inviteHash = data.vinvite_link().v,
		.until = base::unixtime::parse(data.vexpiration_date().v),
		.subscription = Data::PeerSubscription{
			.credits = uint64(data.vpricing().data().vstar_count().v),
			.period = data.vpricing().data().vperiod().v,
		},
		.barePeerId = peerFromTdbChat(data.vchat_id()).value,
		.cancelled = data.vis_canceled().v,
		.expired = (base::unixtime::now() > data.vexpiration_date().v),
		.canRefulfill = data.vcan_reuse().v,
	};
}

[[nodiscard]] Data::CreditsStatusSlice StatusFromTL(
		const TLstarTransactions &status,
		not_null<PeerData*> peer) {
	const auto &data = status.data();
	return Data::CreditsStatusSlice{
		.list = ranges::views::all(
			data.vtransactions().v
		) | ranges::views::transform([&](const TLstarTransaction &tl) {
			return HistoryFromTL(tl, peer);
		}) | ranges::to_vector,
		.balance = uint64(data.vstar_count().v),
		.allLoaded = data.vnext_offset().v.isEmpty(),
		.token = data.vnext_offset().v,
	};
}

[[nodiscard]] Data::CreditsStatusSlice StatusFromTL(
		const TLstarSubscriptions &status,
		not_null<PeerData*> peer) {
	const auto &data = status.data();
	return Data::CreditsStatusSlice{
		.subscriptions = ranges::views::all(
			data.vsubscriptions().v
		) | ranges::views::transform([&](const TLstarSubscription &tl) {
			return SubscriptionFromTL(tl);
		}) | ranges::to_vector,
		.balance = uint64(data.vstar_count().v),
		.allLoaded = data.vnext_offset().v.isEmpty(),
		.token = data.vnext_offset().v,
	};
}

} // namespace

CreditsTopupOptions::CreditsTopupOptions(not_null<PeerData*> peer)
: _peer(peer)
, _api(&peer->session().api().instance()) {
}

rpl::producer<rpl::no_value, QString> CreditsTopupOptions::request() {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

		const auto giftBarePeerId = !_peer->isSelf() ? _peer->id.value : 0;

		const auto optionsFromTL = [giftBarePeerId](const auto &options) {
			return ranges::views::all(
				options
			) | ranges::views::transform([=](const auto &option) {
				const auto &data = option.data();
				return Data::CreditTopupOption{
#if 0 // mtp
					.credits = option.data().vstars().v,
					.product = qs(
						option.data().vstore_product().value_or_empty()),
					.currency = qs(option.data().vcurrency()),
					.amount = option.data().vamount().v,
					.extended = option.data().is_extended(),
#endif
					.credits = uint64(data.vstar_count().v),
					.product = data.vstore_product_id().v,
					.currency = data.vcurrency().v,
					.amount = uint64(data.vamount().v),
					.extended = data.vis_additional().v,
					.giftBarePeerId = giftBarePeerId,
				};
			}) | ranges::to_vector;
		};
#if 0 // mtp
		const auto fail = [=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		};

		if (_peer->isSelf()) {
			using TLOption = MTPStarsTopupOption;
			_api.request(MTPpayments_GetStarsTopupOptions(
			)).done([=](const MTPVector<TLOption> &result) {
				_options = optionsFromTL(result.v);
				consumer.put_done();
			}).fail(fail).send();
		} else if (const auto user = _peer->asUser()) {
			using TLOption = MTPStarsGiftOption;
			_api.request(MTPpayments_GetStarsGiftOptions(
				MTP_flags(MTPpayments_GetStarsGiftOptions::Flag::f_user_id),
				user->inputUser
			)).done([=](const MTPVector<TLOption> &result) {
				_options = optionsFromTL(result.v);
				consumer.put_done();
			}).fail(fail).send();
		}
#endif
		const auto send = [&](auto &&request) {
			_api.request(
				std::move(request)
			).done([=](const TLDstarPaymentOptions &result) {
				_options = optionsFromTL(result.voptions().v);
				consumer.put_done();
			}).fail([=](const Error &error) {
				consumer.put_error_copy(error.message);
			}).send();
		};
		if (_peer->isSelf()) {
			send(TLgetStarPaymentOptions());
		} else if (const auto user = _peer->asUser()) {
			send(TLgetStarGiftPaymentOptions(
				tl_int53(peerToUser(user->id).bare)
			));
		}
		return lifetime;
	};
}

Data::CreditTopupOptions CreditsTopupOptions::options() const {
	return _options;
}

CreditsStatus::CreditsStatus(not_null<PeerData*> peer)
: _peer(peer)
, _api(&peer->session().api().instance()) {
}

void CreditsStatus::request(
		const Data::CreditsStatusSlice::OffsetToken &token,
		Fn<void(Data::CreditsStatusSlice)> done) {
	if (_requestId) {
		return;
	}

#if 0 // mtp
	using TLResult = MTPpayments_StarsStatus;

	_requestId = _api.request(MTPpayments_GetStarsStatus(
		_peer->isSelf() ? MTP_inputPeerSelf() : _peer->input
#endif
	using TLResult = TLstarTransactions;
	_requestId = _api.request(TLgetStarTransactions(
		peerToSender(_peer->id),
		tl_string(), // subscription_id
		null,
		tl_string(),
		tl_int32(kTransactionsPerStatus)
	)).done([=](const TLResult &result) {
		_requestId = 0;
#if 0 // mtp
		const auto balance = result.data().vbalance().v;
#endif
		const auto balance = result.data().vstar_count().v;
		_peer->session().credits().apply(_peer->id, balance);
		if (const auto onstack = done) {
			onstack(StatusFromTL(result, _peer));
		}
	}).fail([=] {
		_requestId = 0;
		if (const auto onstack = done) {
			onstack({});
		}
	}).send();
}

CreditsHistory::CreditsHistory(not_null<PeerData*> peer, bool in, bool out)
: _peer(peer)
#if 0 // mtp
, _flags((in == out)
	? HistoryTL::Flags(0)
	: HistoryTL::Flags(0)
		| (in ? HistoryTL::Flag::f_inbound : HistoryTL::Flags(0))
		| (out ? HistoryTL::Flag::f_outbound : HistoryTL::Flags(0)))
#endif
, _in(in)
, _out(out)
, _api(&peer->session().api().instance()) {
}

void CreditsHistory::request(
		const Data::CreditsStatusSlice::OffsetToken &token,
		Fn<void(Data::CreditsStatusSlice)> done) {
	if (_requestId) {
		return;
	}
#if 0 // mtp
	_requestId = _api.request(MTPpayments_GetStarsTransactions(
		MTP_flags(_flags),
		MTPstring(), // subscription_id
		_peer->isSelf() ? MTP_inputPeerSelf() : _peer->input,
		MTP_string(token),
		MTP_int(kTransactionsLimit)
	)).done([=](const MTPpayments_StarsStatus &result) {
#endif
	_requestId = _api.request(TLgetStarTransactions(
		peerToSender(_peer->id),
		tl_string(), // subscription_id
		((_in == _out)
			? std::optional<TLstarTransactionDirection>()
			: _in
			? tl_starTransactionDirectionIncoming()
			: tl_starTransactionDirectionOutgoing()),
		tl_string(token),
		tl_int32(kTransactionsPerPage)
	)).done([=](const TLstarTransactions &result) {
		_requestId = 0;
		done(StatusFromTL(result, _peer));
	}).fail([=] {
		_requestId = 0;
		done({});
	}).send();
}

void CreditsHistory::requestSubscriptions(
		const Data::CreditsStatusSlice::OffsetToken &token,
		Fn<void(Data::CreditsStatusSlice)> done) {
	if (_requestId) {
		return;
	}
#if 0 // mtp
	_requestId = _api.request(MTPpayments_GetStarsSubscriptions(
		MTP_flags(0),
		_peer->isSelf() ? MTP_inputPeerSelf() : _peer->input,
		MTP_string(token)
	)).done([=](const MTPpayments_StarsStatus &result) {
#endif
	Assert(_peer->isSelf());
	_requestId = _api.request(TLgetStarSubscriptions(
		tl_bool(false), // only_expiring
		tl_string(token)
	)).done([=](const TLstarSubscriptions &result) {
		_requestId = 0;
		done(StatusFromTL(result, _peer));
	}).fail([=] {
		_requestId = 0;
		done({});
	}).send();
}

rpl::producer<not_null<PeerData*>> PremiumPeerBot(
		not_null<Main::Session*> session) {
	return rpl::single(not_null<PeerData*>(session->user()));
#if 0 // mtp this won't be used so just workaround it for now
	const auto username = session->appConfig().get<QString>(
		u"premium_bot_username"_q,
		QString());
	if (username.isEmpty()) {
		return rpl::never<not_null<PeerData*>>();
	}
	if (const auto p = session->data().peerByUsername(username)) {
		return rpl::single<not_null<PeerData*>>(p);
	}
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

		const auto api = lifetime.make_state<MTP::Sender>(&session->mtp());

		api->request(MTPcontacts_ResolveUsername(
			MTP_string(username)
		)).done([=](const MTPcontacts_ResolvedPeer &result) {
			session->data().processUsers(result.data().vusers());
			session->data().processChats(result.data().vchats());
			const auto botPeer = session->data().peerLoaded(
				peerFromMTP(result.data().vpeer()));
			if (!botPeer) {
				return consumer.put_done();
			}
			consumer.put_next(not_null{ botPeer });
		}).send();

		return lifetime;
	};
#endif
}

CreditsEarnStatistics::CreditsEarnStatistics(not_null<PeerData*> peer)
: StatisticsRequestSender(peer)
, _isUser(peer->isUser()) {
}

rpl::producer<rpl::no_value, QString> CreditsEarnStatistics::request() {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

		const auto finish = [=](const QString &url) {
#if 0 // mtp
			makeRequest(MTPpayments_GetStarsRevenueStats(
				MTP_flags(0),
				(_isUser ? user()->input : channel()->input)
			)).done([=](const MTPpayments_StarsRevenueStats &result) {
				const auto &data = result.data();
				const auto &status = data.vstatus().data();
				_data = Data::CreditsEarnStatistics{
					.revenueGraph = StatisticalGraphFromTL(
						data.vrevenue_graph()),
					.currentBalance = status.vcurrent_balance().v,
					.availableBalance = status.vavailable_balance().v,
					.overallRevenue = status.voverall_revenue().v,
					.usdRate = data.vusd_rate().v,
					.isWithdrawalEnabled = status.is_withdrawal_enabled(),
					.nextWithdrawalAt = status.vnext_withdrawal_at()
						? base::unixtime::parse(
							status.vnext_withdrawal_at()->v)
						: QDateTime(),
					.buyAdsUrl = url,
				};

				consumer.put_done();
			}).fail([=](const MTP::Error &error) {
				consumer.put_error_copy(error.type());
			}).send();
#endif
			api().request(TLgetStarRevenueStatistics(
				peerToSender(_isUser ? user()->id : channel()->id),
				tl_bool(false)
			)).done([=](const TLstarRevenueStatistics &result) {
				const auto &data = result.data();
				const auto &status = data.vstatus().data();
				_data = Data::CreditsEarnStatistics{
					.revenueGraph = StatisticalGraphFromTL(
						data.vrevenue_by_day_graph()),
					.currentBalance = uint64(status.vcurrent_count().v),
					.availableBalance = uint64(status.vavailable_count().v),
					.overallRevenue = uint64(status.vtotal_count().v),
					.usdRate = data.vusd_rate().v,
					.isWithdrawalEnabled = status.vwithdrawal_enabled().v,
					.nextWithdrawalAt = (status.vwithdrawal_enabled().v
						? QDateTime::currentDateTime().addSecs(
							status.vnext_withdrawal_in().v)
						: QDateTime()),
					.buyAdsUrl = url,
				};

				consumer.put_done();
			}).fail([=](const Error &error) {
				consumer.put_error_copy(error.message);
			}).send();
		};

#if 0 // mtp
		makeRequest(
			MTPpayments_GetStarsRevenueAdsAccountUrl(
				(_isUser ? user()->input : channel()->input))
		).done([=](const MTPpayments_StarsRevenueAdsAccountUrl &result) {
			finish(qs(result.data().vurl()));
		}).fail([=](const MTP::Error &error) {
			finish({});
		}).send();
#endif
		api().request(TLgetStarAdAccountUrl(
			peerToSender(_isUser ? user()->id : channel()->id)
		)).done([=](const TLhttpUrl &result) {
			finish(result.data().vurl().v);
		}).fail([=](const Error &error) {
			finish({});
		}).send();
		return lifetime;
	};
}

Data::CreditsEarnStatistics CreditsEarnStatistics::data() const {
	return _data;
}

CreditsGiveawayOptions::CreditsGiveawayOptions(not_null<PeerData*> peer)
: _peer(peer)
, _api(&peer->session().api().instance()) {
}

rpl::producer<rpl::no_value, QString> CreditsGiveawayOptions::request() {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

#if 0 // mtp
		using TLOption = MTPStarsGiveawayOption;
#endif

		const auto optionsFromTL = [=](const auto &options) {
			return ranges::views::all(
				options
			) | ranges::views::transform([=](const auto &option) {
				const TLDstarGiveawayPaymentOption &data = option.data();
				return Data::CreditsGiveawayOption{
					.winners = ranges::views::all(
#if 0 // mtp
						option.data().vwinners().v
#endif
						option.data().vwinner_options().v
					) | ranges::views::transform([](const auto &winner) {
						return Data::CreditsGiveawayOption::Winner{
#if 0 // mtp
							.users = winner.data().vusers().v,
							.perUserStars = winner.data().vper_user_stars().v,
							.isDefault = winner.data().is_default(),
#endif
							.users = winner.data().vwinner_count().v,
							.perUserStars = uint64(
								winner.data().vwon_star_count().v),
							.isDefault = winner.data().vis_default().v,
						};
					}) | ranges::to_vector,
#if 0 // mtp
					.storeProduct = qs(
						option.data().vstore_product().value_or_empty()),
					.currency = qs(option.data().vcurrency()),
					.amount = option.data().vamount().v,
					.credits = option.data().vstars().v,
					.yearlyBoosts = option.data().vyearly_boosts().v,
					.isExtended = option.data().is_extended(),
					.isDefault = option.data().is_default(),
#endif
					.storeProduct = data.vstore_product_id().v,
					.currency = data.vcurrency().v,
					.amount = uint64(data.vamount().v),
					.credits = uint64(data.vstar_count().v),
					.yearlyBoosts = data.vyearly_boost_count().v,
					.isExtended = data.vis_additional().v,
					.isDefault = data.vis_default().v,
				};
			}) | ranges::to_vector;
		};
#if 0 // mtp
		const auto fail = [=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		};

		_api.request(MTPpayments_GetStarsGiveawayOptions(
		)).done([=](const MTPVector<TLOption> &result) {
#endif
		const auto fail = [=](const Error &error) {
			consumer.put_error_copy(error.message);
		};
		_api.request(TLgetStarGiveawayPaymentOptions(
		)).done([=](const TLstarGiveawayPaymentOptions &got) {
			const auto &result = got.data().voptions();
			_options = optionsFromTL(result.v);
			consumer.put_done();
		}).fail(fail).send();

		return lifetime;
	};
}

Data::CreditsGiveawayOptions CreditsGiveawayOptions::options() const {
	return _options;
}

} // namespace Api
