/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_statistics.h"

#include "api/api_statistics_data_deserialize.h"
#include "apiwrap.h"
#include "base/unixtime.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "data/data_stories.h"
#include "data/data_story.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"

#include "tdb/tdb_tl_scheme.h"

namespace Api {
namespace {

using namespace Tdb;

#if 0 // mtp
[[nodiscard]] Data::StatisticalValue StatisticalValueFromTL(
		const MTPStatsAbsValueAndPrev &tl) {
	const auto current = tl.data().vcurrent().v;
	const auto previous = tl.data().vprevious().v;
	return Data::StatisticalValue{
		.value = current,
		.previousValue = previous,
		.growthRatePercentage = previous
			? std::abs((current - previous) / float64(previous) * 100.)
			: 0,
	};
}

[[nodiscard]] Data::ChannelStatistics ChannelStatisticsFromTL(
		const MTPDstats_broadcastStats &data) {
	const auto &tlUnmuted = data.venabled_notifications().data();
	const auto unmuted = (!tlUnmuted.vtotal().v)
		? 0.
		: std::clamp(
			tlUnmuted.vpart().v / tlUnmuted.vtotal().v * 100.,
			0.,
			100.);
	using Recent = MTPPostInteractionCounters;
	auto recentMessages = ranges::views::all(
		data.vrecent_posts_interactions().v
	) | ranges::views::transform([&](const Recent &tl) {
		return tl.match([&](const MTPDpostInteractionCountersStory &data) {
			return Data::StatisticsMessageInteractionInfo{
				.storyId = data.vstory_id().v,
				.viewsCount = data.vviews().v,
				.forwardsCount = data.vforwards().v,
				.reactionsCount = data.vreactions().v,
			};
		}, [&](const MTPDpostInteractionCountersMessage &data) {
			return Data::StatisticsMessageInteractionInfo{
				.messageId = data.vmsg_id().v,
				.viewsCount = data.vviews().v,
				.forwardsCount = data.vforwards().v,
				.reactionsCount = data.vreactions().v,
			};
		});
	}) | ranges::to_vector;

	return {
		.startDate = data.vperiod().data().vmin_date().v,
		.endDate = data.vperiod().data().vmax_date().v,

		.memberCount = StatisticalValueFromTL(data.vfollowers()),
		.meanViewCount = StatisticalValueFromTL(data.vviews_per_post()),
		.meanShareCount = StatisticalValueFromTL(data.vshares_per_post()),
		.meanReactionCount = StatisticalValueFromTL(
			data.vreactions_per_post()),

		.meanStoryViewCount = StatisticalValueFromTL(
			data.vviews_per_story()),
		.meanStoryShareCount = StatisticalValueFromTL(
			data.vshares_per_story()),
		.meanStoryReactionCount = StatisticalValueFromTL(
			data.vreactions_per_story()),

		.enabledNotificationsPercentage = unmuted,

		.memberCountGraph = StatisticalGraphFromTL(
			data.vgrowth_graph()),

		.joinGraph = StatisticalGraphFromTL(
			data.vfollowers_graph()),

		.muteGraph = StatisticalGraphFromTL(
			data.vmute_graph()),

		.viewCountByHourGraph = StatisticalGraphFromTL(
			data.vtop_hours_graph()),

		.viewCountBySourceGraph = StatisticalGraphFromTL(
			data.vviews_by_source_graph()),

		.joinBySourceGraph = StatisticalGraphFromTL(
			data.vnew_followers_by_source_graph()),

		.languageGraph = StatisticalGraphFromTL(
			data.vlanguages_graph()),

		.messageInteractionGraph = StatisticalGraphFromTL(
			data.vinteractions_graph()),

		.instantViewInteractionGraph = StatisticalGraphFromTL(
			data.viv_interactions_graph()),

		.reactionsByEmotionGraph = StatisticalGraphFromTL(
			data.vreactions_by_emotion_graph()),

		.storyInteractionsGraph = StatisticalGraphFromTL(
			data.vstory_interactions_graph()),

		.storyReactionsByEmotionGraph = StatisticalGraphFromTL(
			data.vstory_reactions_by_emotion_graph()),

		.recentMessageInteractions = std::move(recentMessages),
	};
}

[[nodiscard]] Data::SupergroupStatistics SupergroupStatisticsFromTL(
		const MTPDstats_megagroupStats &data) {
	using Senders = MTPStatsGroupTopPoster;
	using Administrators = MTPStatsGroupTopAdmin;
	using Inviters = MTPStatsGroupTopInviter;

	auto topSenders = ranges::views::all(
		data.vtop_posters().v
	) | ranges::views::transform([&](const Senders &tl) {
		return Data::StatisticsMessageSenderInfo{
			.userId = UserId(tl.data().vuser_id().v),
			.sentMessageCount = tl.data().vmessages().v,
			.averageCharacterCount = tl.data().vavg_chars().v,
		};
	}) | ranges::to_vector;
	auto topAdministrators = ranges::views::all(
		data.vtop_admins().v
	) | ranges::views::transform([&](const Administrators &tl) {
		return Data::StatisticsAdministratorActionsInfo{
			.userId = UserId(tl.data().vuser_id().v),
			.deletedMessageCount = tl.data().vdeleted().v,
			.bannedUserCount = tl.data().vkicked().v,
			.restrictedUserCount = tl.data().vbanned().v,
		};
	}) | ranges::to_vector;
	auto topInviters = ranges::views::all(
		data.vtop_inviters().v
	) | ranges::views::transform([&](const Inviters &tl) {
		return Data::StatisticsInviterInfo{
			.userId = UserId(tl.data().vuser_id().v),
			.addedMemberCount = tl.data().vinvitations().v,
		};
	}) | ranges::to_vector;

	return {
		.startDate = data.vperiod().data().vmin_date().v,
		.endDate = data.vperiod().data().vmax_date().v,

		.memberCount = StatisticalValueFromTL(data.vmembers()),
		.messageCount = StatisticalValueFromTL(data.vmessages()),
		.viewerCount = StatisticalValueFromTL(data.vviewers()),
		.senderCount = StatisticalValueFromTL(data.vposters()),

		.memberCountGraph = StatisticalGraphFromTL(
			data.vgrowth_graph()),

		.joinGraph = StatisticalGraphFromTL(
			data.vmembers_graph()),

		.joinBySourceGraph = StatisticalGraphFromTL(
			data.vnew_members_by_source_graph()),

		.languageGraph = StatisticalGraphFromTL(
			data.vlanguages_graph()),

		.messageContentGraph = StatisticalGraphFromTL(
			data.vmessages_graph()),

		.actionGraph = StatisticalGraphFromTL(
			data.vactions_graph()),

		.dayGraph = StatisticalGraphFromTL(
			data.vtop_hours_graph()),

		.weekGraph = StatisticalGraphFromTL(
			data.vweekdays_graph()),

		.topSenders = std::move(topSenders),
		.topAdministrators = std::move(topAdministrators),
		.topInviters = std::move(topInviters),
	};
}
#endif

[[nodiscard]] Data::StatisticalValue StatisticalValueFromTL(
		const TLstatisticalValue &tl) {
	return Data::StatisticalValue{
		.value = tl.data().vvalue().v,
		.previousValue = tl.data().vprevious_value().v,
		.growthRatePercentage = tl.data().vgrowth_rate_percentage().v,
	};
}

[[nodiscard]] Data::ChannelStatistics ChannelStatisticsFromTL(
		const TLDchatStatisticsChannel &data) {
	const auto unmuted = data.venabled_notifications_percentage().v;
	using Recent = TLchatStatisticsInteractionInfo;
	auto recentMessages = ranges::views::all(
		data.vrecent_interactions().v
	) | ranges::views::transform([&](const Recent &interaction) {
		const auto &data = interaction.data();
		auto result = Data::StatisticsMessageInteractionInfo{
			.viewsCount = data.vview_count().v,
			.forwardsCount = data.vforward_count().v,
		};
		data.vobject_type().match([&](
				const TLDchatStatisticsObjectTypeMessage &data) {
			result.messageId = data.vmessage_id().v;
		}, [&](const TLDchatStatisticsObjectTypeStory &data) {
			result.storyId = data.vstory_id().v;
		});
		return result;
	}) | ranges::to_vector;

	return {
		.startDate = data.vperiod().data().vstart_date().v,
		.endDate = data.vperiod().data().vend_date().v,

		.memberCount = StatisticalValueFromTL(data.vmember_count()),

		.meanViewCount = StatisticalValueFromTL(
			data.vmean_message_view_count()),
		.meanShareCount = StatisticalValueFromTL(
			data.vmean_message_share_count()),
		.meanReactionCount = StatisticalValueFromTL(
			data.vmean_message_reaction_count()),

		.meanStoryViewCount = StatisticalValueFromTL(
			data.vmean_story_view_count()),
		.meanStoryShareCount = StatisticalValueFromTL(
			data.vmean_story_share_count()),
		.meanStoryReactionCount = StatisticalValueFromTL(
			data.vmean_story_reaction_count()),

		.enabledNotificationsPercentage = unmuted,

		.memberCountGraph = StatisticalGraphFromTL(
			data.vmember_count_graph()),

		.joinGraph = StatisticalGraphFromTL(
			data.vjoin_graph()),

		.muteGraph = StatisticalGraphFromTL(
			data.vmute_graph()),

		.viewCountByHourGraph = StatisticalGraphFromTL(
			data.vview_count_by_hour_graph()),

		.viewCountBySourceGraph = StatisticalGraphFromTL(
			data.vview_count_by_source_graph()),

		.joinBySourceGraph = StatisticalGraphFromTL(
			data.vjoin_by_source_graph()),

		.languageGraph = StatisticalGraphFromTL(
			data.vlanguage_graph()),

		.messageInteractionGraph = StatisticalGraphFromTL(
			data.vmessage_interaction_graph()),

		.instantViewInteractionGraph = StatisticalGraphFromTL(
			data.vinstant_view_interaction_graph()),

		.recentMessageInteractions = std::move(recentMessages),
	};
}

[[nodiscard]] Data::SupergroupStatistics SupergroupStatisticsFromTL(
		const TLDchatStatisticsSupergroup &data) {
	using Senders = TLchatStatisticsMessageSenderInfo;
	using Administrators = TLchatStatisticsAdministratorActionsInfo;
	using Inviters = TLchatStatisticsInviterInfo;

	auto topSenders = ranges::views::all(
		data.vtop_senders().v
	) | ranges::views::transform([&](const Senders &tl) {
		return Data::StatisticsMessageSenderInfo{
			.userId = UserId(tl.data().vuser_id().v),
			.sentMessageCount = tl.data().vsent_message_count().v,
			.averageCharacterCount = tl.data().vaverage_character_count().v,
		};
	}) | ranges::to_vector;
	auto topAdministrators = ranges::views::all(
		data.vtop_administrators().v
	) | ranges::views::transform([&](const Administrators &tl) {
		return Data::StatisticsAdministratorActionsInfo{
			.userId = UserId(tl.data().vuser_id().v),
			.deletedMessageCount = tl.data().vdeleted_message_count().v,
			.bannedUserCount = tl.data().vbanned_user_count().v,
			.restrictedUserCount = tl.data().vrestricted_user_count().v,
		};
	}) | ranges::to_vector;
	auto topInviters = ranges::views::all(
		data.vtop_inviters().v
	) | ranges::views::transform([&](const Inviters &tl) {
		return Data::StatisticsInviterInfo{
			.userId = UserId(tl.data().vuser_id().v),
			.addedMemberCount = tl.data().vadded_member_count().v,
		};
	}) | ranges::to_vector;

	return {
		.startDate = data.vperiod().data().vstart_date().v,
		.endDate = data.vperiod().data().vend_date().v,

		.memberCount = StatisticalValueFromTL(data.vmember_count()),
		.messageCount = StatisticalValueFromTL(data.vmessage_count()),
		.viewerCount = StatisticalValueFromTL(data.vviewer_count()),
		.senderCount = StatisticalValueFromTL(data.vsender_count()),

		.memberCountGraph = StatisticalGraphFromTL(
			data.vmember_count_graph()),

		.joinGraph = StatisticalGraphFromTL(
			data.vjoin_graph()),

		.joinBySourceGraph = StatisticalGraphFromTL(
			data.vjoin_by_source_graph()),

		.languageGraph = StatisticalGraphFromTL(
			data.vlanguage_graph()),

		.messageContentGraph = StatisticalGraphFromTL(
			data.vmessage_content_graph()),

		.actionGraph = StatisticalGraphFromTL(
			data.vaction_graph()),

		.dayGraph = StatisticalGraphFromTL(
			data.vday_graph()),

		.weekGraph = StatisticalGraphFromTL(
			data.vweek_graph()),

		.topSenders = std::move(topSenders),
		.topAdministrators = std::move(topAdministrators),
		.topInviters = std::move(topInviters),
	};
}

} // namespace

Statistics::Statistics(not_null<ChannelData*> channel)
: StatisticsRequestSender(channel) {
}

rpl::producer<rpl::no_value, QString> Statistics::request() {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

#if 0 // mtp
		if (!channel()->isMegagroup()) {
			makeRequest(MTPstats_GetBroadcastStats(
				MTP_flags(MTPstats_GetBroadcastStats::Flags(0)),
				channel()->inputChannel
			)).done([=](const MTPstats_BroadcastStats &result) {
				_channelStats = ChannelStatisticsFromTL(result.data());
				consumer.put_done();
			}).fail([=](const MTP::Error &error) {
				consumer.put_error_copy(error.type());
			}).send();
		} else {
			makeRequest(MTPstats_GetMegagroupStats(
				MTP_flags(MTPstats_GetMegagroupStats::Flags(0)),
				channel()->inputChannel
			)).done([=](const MTPstats_MegagroupStats &result) {
				const auto &data = result.data();
				_supergroupStats = SupergroupStatisticsFromTL(data);
				channel()->owner().processUsers(data.vusers());
				consumer.put_done();
			}).fail([=](const MTP::Error &error) {
				consumer.put_error_copy(error.type());
			}).send();
		}
#endif
		api().request(TLgetChatStatistics(
			peerToTdbChat(channel()->id),
			tl_bool(false)
		)).done([=](const TLchatStatistics &result) {
			result.match([&](const TLDchatStatisticsChannel &data) {
				_channelStats = ChannelStatisticsFromTL(data);
			}, [&](const TLDchatStatisticsSupergroup &data) {
				_supergroupStats = SupergroupStatisticsFromTL(data);
			});
			consumer.put_done();
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();

		return lifetime;
	};
}

Statistics::GraphResult Statistics::requestZoom(
		const QString &token,
		float64 x) {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();
		const auto wasEmpty = _zoomDeque.empty();
		_zoomDeque.push_back([=] {
#if 0 // mtp
			makeRequest(MTPstats_LoadAsyncGraph(
				MTP_flags(x
					? MTPstats_LoadAsyncGraph::Flag::f_x
					: MTPstats_LoadAsyncGraph::Flag(0)),
				MTP_string(token),
				MTP_long(x)
			)).done([=](const MTPStatsGraph &result) {
				consumer.put_next(StatisticalGraphFromTL(result));
				consumer.put_done();
				if (!_zoomDeque.empty()) {
					_zoomDeque.pop_front();
					if (!_zoomDeque.empty()) {
						_zoomDeque.front()();
					}
				}
			}).fail([=](const MTP::Error &error) {
				consumer.put_error_copy(error.type());
			}).send();
#endif
			api().request(TLgetStatisticalGraph(
				peerToTdbChat(channel()->id),
				tl_string(token),
				tl_int53(x)
			)).done([=](const TLstatisticalGraph &result) {
				consumer.put_next(StatisticalGraphFromTL(result));
				consumer.put_done();
				if (!_zoomDeque.empty()) {
					_zoomDeque.pop_front();
					if (!_zoomDeque.empty()) {
						_zoomDeque.front()();
					}
				}
			}).fail([=](const Error &error) {
				consumer.put_error_copy(error.message);
			}).send();
		});
		if (wasEmpty) {
			_zoomDeque.front()();
		}

		return lifetime;
	};
}

Data::ChannelStatistics Statistics::channelStats() const {
	return _channelStats;
}

Data::SupergroupStatistics Statistics::supergroupStats() const {
	return _supergroupStats;
}

PublicForwards::PublicForwards(
	not_null<ChannelData*> channel,
	Data::RecentPostId fullId)
: StatisticsRequestSender(channel)
, _fullId(fullId) {
}

void PublicForwards::request(
		const Data::PublicForwardsSlice::OffsetToken &token,
		Fn<void(Data::PublicForwardsSlice)> done) {
	if (_requestId) {
		return;
	}
	const auto channel = StatisticsRequestSender::channel();

	constexpr auto kLimit = tl::make_int(100);
	const auto processResult = [=](const TLpublicForwards &result) {
		using Messages = QVector<Data::RecentPostId>;
		_requestId = 0;

		const auto &data = result.data();
		auto nextToken = data.vnext_offset().v;
		auto recentList = Messages();
		recentList.reserve(data.vforwards().v.size());
		for (const auto &forward : result.data().vforwards().v) {
			forward.match([&](const TLDpublicForwardMessage &data) {
				recentList.push_back({
					.messageId = channel->owner().processMessage(
						data.vmessage(),
						NewMessageType::Existing)->fullId(),
				});
			}, [&](const TLDpublicForwardStory &data) {
				const auto story = channel->owner().stories().applySingle(
					data.vstory());
				if (story) {
					recentList.push_back({ .storyId = story->fullId() });
				}
			});
		}

		auto allLoaded = false;
		auto fullCount = result.data().vtotal_count().v;

		if (nextToken == token || nextToken.isEmpty()) {
			allLoaded = true;
		}

		_lastTotal = std::max(_lastTotal, fullCount);
		done({
			.list = std::move(recentList),
			.total = _lastTotal,
			.allLoaded = allLoaded,
			.token = std::move(nextToken),
		});
	};
	if (_fullId.messageId) {
		_requestId = api().request(TLgetMessagePublicForwards(
			peerToTdbChat(channel->id),
			tl_int53(_fullId.messageId.msg.bare),
			tl_string(token),
			kLimit
		)).done(processResult).fail([=] { _requestId = 0; }).send();
	} else if (_fullId.storyId) {
		_requestId = api().request(TLgetStoryPublicForwards(
			peerToTdbChat(channel->id),
			tl_int32(_fullId.storyId.story),
			tl_string(token),
			kLimit
		)).done(processResult).fail([=] { _requestId = 0; }).send();
	}
#if 0 // mtp
	const auto processResult = [=](const MTPstats_PublicForwards &tl) {
		using Messages = QVector<Data::RecentPostId>;
		_requestId = 0;

		const auto &data = tl.data();
		auto &owner = channel->owner();

		owner.processUsers(data.vusers());
		owner.processChats(data.vchats());

		const auto nextToken = data.vnext_offset()
			? qs(*data.vnext_offset())
			: Data::PublicForwardsSlice::OffsetToken();

		const auto fullCount = data.vcount().v;

		auto recentList = Messages(data.vforwards().v.size());
		for (const auto &tlForward : data.vforwards().v) {
			tlForward.match([&](const MTPDpublicForwardMessage &data) {
				const auto &message = data.vmessage();
				const auto msgId = IdFromMessage(message);
				const auto peerId = PeerFromMessage(message);
				const auto lastDate = DateFromMessage(message);
				if (const auto peer = owner.peerLoaded(peerId)) {
					if (!lastDate) {
						return;
					}
					owner.addNewMessage(
						message,
						MessageFlags(),
						NewMessageType::Existing);
					recentList.push_back({ .messageId = { peerId, msgId } });
				}
			}, [&](const MTPDpublicForwardStory &data) {
				const auto story = owner.stories().applySingle(
					peerFromMTP(data.vpeer()),
					data.vstory());
				if (story) {
					recentList.push_back({ .storyId = story->fullId() });
				}
			});
		}

		const auto allLoaded = nextToken.isEmpty() || (nextToken == token);
		_lastTotal = std::max(_lastTotal, fullCount);
		done({
			.list = std::move(recentList),
			.total = _lastTotal,
			.allLoaded = allLoaded,
			.token = nextToken,
		});
	};

	constexpr auto kLimit = tl::make_int(100);
	if (_fullId.messageId) {
		_requestId = makeRequest(MTPstats_GetMessagePublicForwards(
			channel->inputChannel,
			MTP_int(_fullId.messageId.msg),
			MTP_string(token),
			kLimit
		)).done(processResult).fail([=] { _requestId = 0; }).send();
	} else if (_fullId.storyId) {
		_requestId = makeRequest(MTPstats_GetStoryPublicForwards(
			channel->input,
			MTP_int(_fullId.storyId.story),
			MTP_string(token),
			kLimit
		)).done(processResult).fail([=] { _requestId = 0; }).send();
	}
#endif
}

MessageStatistics::MessageStatistics(
	not_null<ChannelData*> channel,
	FullMsgId fullId)
: StatisticsRequestSender(channel)
, _publicForwards(channel, { .messageId = fullId })
, _fullId(fullId) {
}

MessageStatistics::MessageStatistics(
	not_null<ChannelData*> channel,
	FullStoryId storyId)
: StatisticsRequestSender(channel)
, _publicForwards(channel, { .storyId = storyId })
, _storyId(storyId) {
}

Data::PublicForwardsSlice MessageStatistics::firstSlice() const {
	return _firstSlice;
}

void MessageStatistics::request(Fn<void(Data::MessageStatistics)> done) {
	if (channel()->isMegagroup()) {
		return;
	}
	const auto requestFirstPublicForwards = [=](
			const Data::StatisticalGraph &messageGraph,
			const Data::StatisticalGraph &reactionsGraph,
			const Data::StatisticsMessageInteractionInfo &info) {
		const auto callback = [=](Data::PublicForwardsSlice slice) {
			const auto total = slice.total;
			_firstSlice = std::move(slice);
			done({
				.messageInteractionGraph = messageGraph,
				.reactionsByEmotionGraph = reactionsGraph,
				.publicForwards = total,
				.privateForwards = info.forwardsCount - total,
				.views = info.viewsCount,
				.reactions = info.reactionsCount,
			});
		};
		_publicForwards.request({}, callback);
	};

	const auto requestPrivateForwards = [=](
			const Data::StatisticalGraph &messageGraph,
			const Data::StatisticalGraph &reactionsGraph) {
#if 0 // mtp
		api().request(MTPchannels_GetMessages(
			channel()->inputChannel,
			MTP_vector<MTPInputMessage>(
				1,
				MTP_inputMessageID(MTP_int(_fullId.msg))))
		).done([=](const MTPmessages_Messages &result) {
			const auto process = [&](const MTPVector<MTPMessage> &messages) {
				const auto &message = messages.v.front();
				return message.match([&](const MTPDmessage &data) {
					auto reactionsCount = 0;
					if (const auto tlReactions = data.vreactions()) {
						const auto &tlCounts = tlReactions->data().vresults();
						for (const auto &tlCount : tlCounts.v) {
							reactionsCount += tlCount.data().vcount().v;
						}
					}
					return Data::StatisticsMessageInteractionInfo{
						.messageId = IdFromMessage(message),
						.viewsCount = data.vviews()
							? data.vviews()->v
							: 0,
						.forwardsCount = data.vforwards()
							? data.vforwards()->v
							: 0,
						.reactionsCount = reactionsCount,
					};
				}, [](const MTPDmessageEmpty &) {
					return Data::StatisticsMessageInteractionInfo();
				}, [](const MTPDmessageService &) {
					return Data::StatisticsMessageInteractionInfo();
				});
			};

			auto info = result.match([&](const MTPDmessages_messages &data) {
				return process(data.vmessages());
			}, [&](const MTPDmessages_messagesSlice &data) {
				return process(data.vmessages());
			}, [&](const MTPDmessages_channelMessages &data) {
				return process(data.vmessages());
			}, [](const MTPDmessages_messagesNotModified &) {
				return Data::StatisticsMessageInteractionInfo();
			});

			requestFirstPublicForwards(
				messageGraph,
				reactionsGraph,
				std::move(info));
		}).fail([=](const MTP::Error &error) {
			requestFirstPublicForwards(messageGraph, reactionsGraph, {});
		}).send();
#endif
		api().request(TLgetMessage(
			peerToTdbChat(channel()->id),
			tl_int53(_fullId.msg.bare)
		)).done([=](const TLDmessage &data) {
			auto info = Data::StatisticsMessageInteractionInfo{
				.messageId = data.vid().v,
				.viewsCount = data.vinteraction_info()
					? data.vinteraction_info()->data().vview_count().v
					: 0,
				.forwardsCount = data.vinteraction_info()
					? data.vinteraction_info()->data().vforward_count().v
					: 0,
			};

			requestFirstPublicForwards(
				messageGraph,
				reactionsGraph,
				std::move(info));
		}).fail([=](const Error &error) {
			requestFirstPublicForwards(messageGraph, reactionsGraph, {});
		}).send();
	};

	const auto requestStoryPrivateForwards = [=](
			const Data::StatisticalGraph &messageGraph,
			const Data::StatisticalGraph &reactionsGraph) {
		api().request(TLgetStory(
			peerToTdbChat(channel()->id),
			tl_int32(_storyId.story),
			tl_bool(false)
		)).done([=](const TLstory &result) {
			auto info = Data::StatisticsMessageInteractionInfo();
			const auto &data = result.data();
			if (const auto interaction = data.vinteraction_info()) {
				const auto &data = interaction->data();
				info = Data::StatisticsMessageInteractionInfo{
					.storyId = result.data().vid().v,
					.viewsCount = data.vview_count().v,
					.forwardsCount = data.vforward_count().v,
					.reactionsCount = data.vreaction_count().v,
				};
			}
			requestFirstPublicForwards(
				messageGraph,
				reactionsGraph,
				std::move(info));
		}).fail([=](const Error &error) {
			requestFirstPublicForwards(messageGraph, reactionsGraph, {});
		}).send();
	};
	if (_storyId) {
		api().request(TLgetStoryStatistics(
			peerToTdbChat(channel()->id),
			tl_int32(_storyId.story),
			tl_bool(false)
		)).done([=](const TLDstoryStatistics &data) {
			requestStoryPrivateForwards(
				StatisticalGraphFromTL(data.vstory_interaction_graph()),
				StatisticalGraphFromTL(data.vstory_reaction_graph()));
		}).fail([=](const Error &error) {
			requestStoryPrivateForwards({}, {});
		}).send();
	} else {
		api().request(TLgetMessageStatistics(
			peerToTdbChat(channel()->id),
			tl_int53(_fullId.msg.bare),
			tl_bool(false)
		)).done([=](const TLDmessageStatistics &data) {
			requestPrivateForwards(
				StatisticalGraphFromTL(data.vmessage_interaction_graph()),
				StatisticalGraphFromTL(data.vmessage_reaction_graph()));
		}).fail([=](const Error &error) {
			requestPrivateForwards({}, {});
		}).send();
	}
#if 0 // mtp
	const auto requestStoryPrivateForwards = [=](
			const Data::StatisticalGraph &messageGraph,
			const Data::StatisticalGraph &reactionsGraph) {
		api().request(MTPstories_GetStoriesByID(
			channel()->input,
			MTP_vector<MTPint>(1, MTP_int(_storyId.story)))
		).done([=](const MTPstories_Stories &result) {
			const auto &storyItem = result.data().vstories().v.front();
			auto info = storyItem.match([&](const MTPDstoryItem &data) {
				if (!data.vviews()) {
					return Data::StatisticsMessageInteractionInfo();
				}
				const auto &tlViews = data.vviews()->data();
				return Data::StatisticsMessageInteractionInfo{
					.storyId = data.vid().v,
					.viewsCount = tlViews.vviews_count().v,
					.forwardsCount = tlViews.vforwards_count().value_or(0),
					.reactionsCount = tlViews.vreactions_count().value_or(0),
				};
			}, [](const auto &) {
				return Data::StatisticsMessageInteractionInfo();
			});

			requestFirstPublicForwards(
				messageGraph,
				reactionsGraph,
				std::move(info));
		}).fail([=](const MTP::Error &error) {
			requestFirstPublicForwards(messageGraph, reactionsGraph, {});
		}).send();
	};

	if (_storyId) {
		makeRequest(MTPstats_GetStoryStats(
			MTP_flags(MTPstats_GetStoryStats::Flags(0)),
			channel()->input,
			MTP_int(_storyId.story)
		)).done([=](const MTPstats_StoryStats &result) {
			const auto &data = result.data();
			requestStoryPrivateForwards(
				StatisticalGraphFromTL(data.vviews_graph()),
				StatisticalGraphFromTL(data.vreactions_by_emotion_graph()));
		}).fail([=](const MTP::Error &error) {
			requestStoryPrivateForwards({}, {});
		}).send();
	} else {
		makeRequest(MTPstats_GetMessageStats(
			MTP_flags(MTPstats_GetMessageStats::Flags(0)),
			channel()->inputChannel,
			MTP_int(_fullId.msg.bare)
		)).done([=](const MTPstats_MessageStats &result) {
			const auto &data = result.data();
			requestPrivateForwards(
				StatisticalGraphFromTL(data.vviews_graph()),
				StatisticalGraphFromTL(data.vreactions_by_emotion_graph()));
		}).fail([=](const MTP::Error &error) {
			requestPrivateForwards({}, {});
		}).send();
	}
#endif
}

Boosts::Boosts(not_null<PeerData*> peer)
: _peer(peer)
, _api(&peer->session().api().instance()) {
}

rpl::producer<rpl::no_value, QString> Boosts::request() {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();
		const auto channel = _peer->asChannel();
		if (!channel) {
			return lifetime;
		}

#if 0 // mtp
		_api.request(MTPpremium_GetBoostsStatus(
			_peer->input
		)).done([=](const MTPpremium_BoostsStatus &result) {
			const auto &data = result.data();
			channel->updateLevelHint(data.vlevel().v);
			const auto hasPremium = !!data.vpremium_audience();
			const auto premiumMemberCount = hasPremium
				? std::max(0, int(data.vpremium_audience()->data().vpart().v))
				: 0;
			const auto participantCount = hasPremium
				? std::max(
					int(data.vpremium_audience()->data().vtotal().v),
					premiumMemberCount)
				: 0;
			const auto premiumMemberPercentage = (participantCount > 0)
				? (100. * premiumMemberCount / participantCount)
				: 0;

			const auto slots = data.vmy_boost_slots();
			_boostStatus.overview = Data::BoostsOverview{
				.group = channel->isMegagroup(),
				.mine = slots ? int(slots->v.size()) : 0,
				.level = std::max(data.vlevel().v, 0),
				.boostCount = std::max(
					data.vboosts().v,
					data.vcurrent_level_boosts().v),
				.currentLevelBoostCount = data.vcurrent_level_boosts().v,
				.nextLevelBoostCount = data.vnext_level_boosts()
					? data.vnext_level_boosts()->v
					: 0,
				.premiumMemberCount = premiumMemberCount,
				.premiumMemberPercentage = premiumMemberPercentage,
			};
			_boostStatus.link = qs(data.vboost_url());

			if (data.vprepaid_giveaways()) {
				_boostStatus.prepaidGiveaway = ranges::views::all(
					data.vprepaid_giveaways()->v
				) | ranges::views::transform([](const MTPPrepaidGiveaway &r) {
					return r.match([&](const MTPDprepaidGiveaway &data) {
						return Data::BoostPrepaidGiveaway{
							.date = base::unixtime::parse(data.vdate().v),
							.id = data.vid().v,
							.months = data.vmonths().v,
							.quantity = data.vquantity().v,
						};
					}, [&](const MTPDprepaidStarsGiveaway &data) {
						return Data::BoostPrepaidGiveaway{
							.date = base::unixtime::parse(data.vdate().v),
							.id = data.vid().v,
							.credits = data.vstars().v,
							.quantity = data.vquantity().v,
							.boosts = data.vboosts().v,
						};
					});
				}) | ranges::to_vector;
			}

			using namespace Data;
			requestBoosts({ .gifts = false }, [=](BoostsListSlice &&slice) {
				_boostStatus.firstSliceBoosts = std::move(slice);
				requestBoosts({ .gifts = true }, [=](BoostsListSlice &&s) {
					_boostStatus.firstSliceGifts = std::move(s);
					consumer.put_done();
				});
			});
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
#endif
		_api.request(TLgetChatBoostStatus(
			peerToTdbChat(_peer->id)
		)).done([=](const TLDchatBoostStatus &data) {
			_boostStatus.overview = Data::BoostsOverview{
				.mine = bool(data.vapplied_slot_ids().v.size()),
				.level = std::max(data.vlevel().v, 0),
				.boostCount = std::max(
					data.vboost_count().v,
					data.vcurrent_level_boost_count().v),
				.currentLevelBoostCount = data.vcurrent_level_boost_count().v,
				.nextLevelBoostCount = data.vnext_level_boost_count().v,
				.premiumMemberCount = data.vpremium_member_count().v,
				.premiumMemberPercentage =
					data.vpremium_member_percentage().v,
			};
			_boostStatus.link = data.vboost_url().v.toUtf8();

			_boostStatus.prepaidGiveaway = ranges::views::all(
				data.vprepaid_giveaways().v
			) | ranges::views::transform([](
					const TLprepaidGiveaway &r) {
				auto months = 0;
				auto stars = uint64();
				r.data().vprize().match([&](
						const TLDgiveawayPrizePremium &data) {
					months = data.vmonth_count().v;
				}, [&](const TLDgiveawayPrizeStars &data) {
					stars = data.vstar_count().v;
				});
				return Data::BoostPrepaidGiveaway{
					.date = QDateTime::fromSecsSinceEpoch(
						r.data().vpayment_date().v),
					.id = uint64(r.data().vid().v),
					.credits = stars,
					.months = months,
					.quantity = r.data().vwinner_count().v,
					.boosts = r.data().vboost_count().v,
				};
			}) | ranges::to_vector;

			using namespace Data;
			requestBoosts({ .gifts = false }, [=](BoostsListSlice &&slice) {
				_boostStatus.firstSliceBoosts = std::move(slice);
				requestBoosts({ .gifts = true }, [=](BoostsListSlice &&s) {
					_boostStatus.firstSliceGifts = std::move(s);
					consumer.put_done();
				});
			});
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();

		return lifetime;
	};
}

void Boosts::requestBoosts(
		const Data::BoostsListSlice::OffsetToken &token,
		Fn<void(Data::BoostsListSlice)> done) {
	if (_requestId) {
		return;
	}
	constexpr auto kTlFirstSlice = tl::make_int(kFirstSlice);
	constexpr auto kTlLimit = tl::make_int(kLimit);
	const auto gifts = token.gifts;
	_requestId = _api.request(TLgetChatBoosts(
		peerToTdbChat(_peer->id),
		tl_bool(gifts),
		tl_string(token.next),
		token.next.isEmpty() ? kTlFirstSlice : kTlLimit
	)).done([=](const TLDfoundChatBoosts &data) {
		_requestId = 0;

		auto list = std::vector<Data::Boost>();
		list.reserve(data.vboosts().v.size());
		constexpr auto kMonthsDivider = int(30 * 86400);
		for (const auto &boost : data.vboosts().v) {
			const auto &data = boost.data();
			const auto userId = data.vsource().match([&](const auto &d) {
				return UserId(d.vuser_id().v);
			});
			const auto giftCodeLink = data.vsource().match([](
					const TLDchatBoostSourcePremium &) {
				return Data::GiftCodeLink();
			}, [&](const auto &d) {
				const auto tlGiftCode = d.vgift_code().v.toUtf8();
				const auto path = !tlGiftCode.isEmpty()
					? (u"giftcode/"_q + tlGiftCode)
					: QString();
				return Data::GiftCodeLink{
					_peer->session().createInternalLink(path),
					_peer->session().createInternalLinkFull(path),
					tlGiftCode,
				};
			});
			const auto isUnclaimed = data.vsource().match([](
					const TLDchatBoostSourceGiveaway &d) {
				return d.vis_unclaimed().v;
			}, [](const auto &) {
				return false;
			});
			const auto giveawayMessage = data.vsource().match([&](
					const TLDchatBoostSourceGiveaway &d) {
				return FullMsgId{ _peer->id, d.vgiveaway_message_id().v };
			}, [](const auto &) {
				return FullMsgId();
			});

			const auto stars = data.vsource().match([&](
					const TLDchatBoostSourceGiveaway &d) {
				return d.vstar_count().v;
			}, [&](const auto &) {
				return int64();
			});

			list.push_back({
				.id = data.vid().v.toUtf8(),
				.userId = userId,
				.giveawayMessage = giveawayMessage,
				.date = QDateTime::fromSecsSinceEpoch(data.vstart_date().v),
				.expiresAt = QDateTime::fromSecsSinceEpoch(data.vexpiration_date().v),
				.expiresAfterMonths = ((data.vexpiration_date().v - data.vstart_date().v)
					/ kMonthsDivider),
				.giftCodeLink = std::move(giftCodeLink),
				.multiplier = (data.vcount().v == 1 ? 0 : data.vcount().v),
				.credits = uint64(stars),

				.isGift = !giftCodeLink.slug.isEmpty(),
				.isGiveaway = (!!giveawayMessage),
				.isUnclaimed = isUnclaimed,
			});
		}
		done(Data::BoostsListSlice{
			.list = std::move(list),
			.multipliedTotal = data.vtotal_count().v,
			.allLoaded = (data.vtotal_count().v == data.vboosts().v.size()),
			.token = Data::BoostsListSlice::OffsetToken{
				.next = data.vnext_offset().v.toUtf8(),
				.gifts = gifts,
			},
		});
	}).fail([=] {
		_requestId = 0;
	}).send();
#if 0 // mtp
	_requestId = _api.request(MTPpremium_GetBoostsList(
		gifts
			? MTP_flags(MTPpremium_GetBoostsList::Flag::f_gifts)
			: MTP_flags(0),
		_peer->input,
		MTP_string(token.next),
		token.next.isEmpty() ? kTlFirstSlice : kTlLimit
	)).done([=](const MTPpremium_BoostsList &result) {
		_requestId = 0;

		const auto &data = result.data();
		_peer->owner().processUsers(data.vusers());

		auto list = std::vector<Data::Boost>();
		list.reserve(data.vboosts().v.size());
		constexpr auto kMonthsDivider = int(30 * 86400);
		for (const auto &boost : data.vboosts().v) {
			const auto &data = boost.data();
			const auto path = data.vused_gift_slug()
				? (u"giftcode/"_q + qs(data.vused_gift_slug()->v))
				: QString();
			auto giftCodeLink = !path.isEmpty()
				? Data::GiftCodeLink{
					_peer->session().createInternalLink(path),
					_peer->session().createInternalLinkFull(path),
					qs(data.vused_gift_slug()->v),
				}
				: Data::GiftCodeLink();
			list.push_back({
				.id = qs(data.vid()),
				.userId = UserId(data.vuser_id().value_or_empty()),
				.giveawayMessage = data.vgiveaway_msg_id()
					? FullMsgId{ _peer->id, data.vgiveaway_msg_id()->v }
					: FullMsgId(),
				.date = base::unixtime::parse(data.vdate().v),
				.expiresAt = base::unixtime::parse(data.vexpires().v),
				.expiresAfterMonths = ((data.vexpires().v - data.vdate().v)
					/ kMonthsDivider),
				.giftCodeLink = std::move(giftCodeLink),
				.multiplier = data.vmultiplier().value_or_empty(),
				.credits = data.vstars().value_or_empty(),
				.isGift = data.is_gift(),
				.isGiveaway = data.is_giveaway(),
				.isUnclaimed = data.is_unclaimed(),
			});
		}
		done(Data::BoostsListSlice{
			.list = std::move(list),
			.multipliedTotal = data.vcount().v,
			.allLoaded = (data.vcount().v == data.vboosts().v.size()),
			.token = Data::BoostsListSlice::OffsetToken{
				.next = data.vnext_offset()
					? qs(*data.vnext_offset())
					: QString(),
				.gifts = gifts,
			},
		});
	}).fail([=] {
		_requestId = 0;
	}).send();
#endif
}

Data::BoostStatus Boosts::boostStatus() const {
	return _boostStatus;
}

ChannelEarnStatistics::ChannelEarnStatistics(not_null<ChannelData*> channel)
: StatisticsRequestSender(channel) {
}

rpl::producer<rpl::no_value, QString> ChannelEarnStatistics::request() {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

		api().request(TLgetChatRevenueStatistics(
			peerToTdbChat(channel()->id),
			tl_bool(false)
		)).done([=](const TLchatRevenueStatistics &result) {
			const auto &data = result.data();
			const auto &amount = data.vrevenue_amount().data();
			_data = Data::EarnStatistics{
				.topHoursGraph = StatisticalGraphFromTL(
					data.vrevenue_by_hour_graph()),
				.revenueGraph = StatisticalGraphFromTL(
					data.vrevenue_graph()),
				.currentBalance = uint64(amount.vbalance_amount().v),
				.availableBalance = uint64(amount.vavailable_amount().v),
				.overallRevenue = uint64(amount.vtotal_amount().v),
				.usdRate = data.vusd_rate().v,
			};
			requestHistory({}, [=](Data::EarnHistorySlice &&slice) {
				_data.firstHistorySlice = std::move(slice);

				api().request(TLgetSupergroupFullInfo(
					tl_int53(peerToChannel(channel()->id).bare)
				)).done([=](const TLsupergroupFullInfo &result) {
					_data.switchedOff
						= !result.data().vcan_have_sponsored_messages().v;
					consumer.put_done();
				}).fail([=](const Error &error) {
					consumer.put_error_copy(error.message);
				}).send();
			});
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();
#if 0 // mtp
		makeRequest(MTPstats_GetBroadcastRevenueStats(
			MTP_flags(0),
			channel()->inputChannel
		)).done([=](const MTPstats_BroadcastRevenueStats &result) {
			const auto &data = result.data();
			const auto &balances = data.vbalances().data();
			_data = Data::EarnStatistics{
				.topHoursGraph = StatisticalGraphFromTL(
					data.vtop_hours_graph()),
				.revenueGraph = StatisticalGraphFromTL(data.vrevenue_graph()),
				.currentBalance = balances.vcurrent_balance().v,
				.availableBalance = balances.vavailable_balance().v,
				.overallRevenue = balances.voverall_revenue().v,
				.usdRate = data.vusd_rate().v,
			};

			requestHistory({}, [=](Data::EarnHistorySlice &&slice) {
				_data.firstHistorySlice = std::move(slice);

				api().request(
					MTPchannels_GetFullChannel(channel()->inputChannel)
				).done([=](const MTPmessages_ChatFull &result) {
					result.data().vfull_chat().match([&](
							const MTPDchannelFull &d) {
						_data.switchedOff = d.is_restricted_sponsored();
					}, [](const auto &) {
					});
					consumer.put_done();
				}).fail([=](const MTP::Error &error) {
					consumer.put_error_copy(error.type());
				}).send();
			});
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
#endif

		return lifetime;
	};
}

void ChannelEarnStatistics::requestHistory(
		const Data::EarnHistorySlice::OffsetToken &token,
		Fn<void(Data::EarnHistorySlice)> done) {
	if (_requestId) {
		return;
	}
	constexpr auto kTlFirstSlice = tl::make_int(kFirstSlice);
	constexpr auto kTlLimit = tl::make_int(kLimit);
#if 0 // mtp
	_requestId = api().request(MTPstats_GetBroadcastRevenueTransactions(
		channel()->inputChannel,
		MTP_int(token),
		(!token) ? kTlFirstSlice : kTlLimit
	)).done([=](const MTPstats_BroadcastRevenueTransactions &result) {
		_requestId = 0;

		const auto &tlTransactions = result.data().vtransactions().v;

		auto list = std::vector<Data::EarnHistoryEntry>();
		list.reserve(tlTransactions.size());
		for (const auto &tlTransaction : tlTransactions) {
			list.push_back(tlTransaction.match([&](
					const MTPDbroadcastRevenueTransactionProceeds &d) {
				return Data::EarnHistoryEntry{
					.type = Data::EarnHistoryEntry::Type::In,
					.amount = d.vamount().v,
					.date = base::unixtime::parse(d.vfrom_date().v),
					.dateTo = base::unixtime::parse(d.vto_date().v),
				};
			}, [&](const MTPDbroadcastRevenueTransactionWithdrawal &d) {
				return Data::EarnHistoryEntry{
					.type = Data::EarnHistoryEntry::Type::Out,
					.status = d.is_pending()
						? Data::EarnHistoryEntry::Status::Pending
						: d.is_failed()
						? Data::EarnHistoryEntry::Status::Failed
						: Data::EarnHistoryEntry::Status::Success,
					.amount = (std::numeric_limits<Data::EarnInt>::max()
						- d.vamount().v
						+ 1),
					.date = base::unixtime::parse(d.vdate().v),
					// .provider = qs(d.vprovider()),
					.successDate = d.vtransaction_date()
						? base::unixtime::parse(d.vtransaction_date()->v)
						: QDateTime(),
					.successLink = d.vtransaction_url()
						? qs(*d.vtransaction_url())
						: QString(),
				};
			}, [&](const MTPDbroadcastRevenueTransactionRefund &d) {
				return Data::EarnHistoryEntry{
					.type = Data::EarnHistoryEntry::Type::Return,
					.amount = d.vamount().v,
					.date = base::unixtime::parse(d.vdate().v),
					// .provider = qs(d.vprovider()),
				};
			}));
		}
		const auto nextToken = token + tlTransactions.size();
		done(Data::EarnHistorySlice{
			.list = std::move(list),
			.total = result.data().vcount().v,
			.allLoaded = (result.data().vcount().v == nextToken),
			.token = Data::EarnHistorySlice::OffsetToken(nextToken),
		});
#endif
	_requestId = api().request(TLgetChatRevenueTransactions(
		peerToTdbChat(channel()->id),
		tl_int32(token),
		(!token) ? kTlFirstSlice : kTlLimit
	)).done([=](const TLchatRevenueTransactions &result) {
		_requestId = 0;

		const auto &data = result.data();
		const auto &tlTransactions = data.vtransactions().v;

		auto list = std::vector<Data::EarnHistoryEntry>();
		list.reserve(tlTransactions.size());
		for (const auto &tlTransaction : tlTransactions) {
			const auto &d = tlTransaction.data();
			list.push_back({
				.amount = uint64(d.vcryptocurrency_amount().v),
			});
			auto &entry = list.back();
			d.vtype().match([&](
					const TLDchatRevenueTransactionTypeEarnings &d) {
				entry.type = Data::EarnHistoryEntry::Type::In;
				entry.date = base::unixtime::parse(d.vstart_date().v);
				entry.dateTo = base::unixtime::parse(d.vend_date().v);
			}, [&](const TLDchatRevenueTransactionTypeWithdrawal &d) {
				entry.type = Data::EarnHistoryEntry::Type::Out;
				entry.amount = (std::numeric_limits<Data::EarnInt>::max()
					- entry.amount
					+ 1);
				entry.date = base::unixtime::parse(d.vwithdrawal_date().v);
				// entry.provider = d.vprovider().v;
				d.vstate().match([&](const TLDrevenueWithdrawalStateFailed &d) {
					entry.status = Data::EarnHistoryEntry::Status::Failed;
				}, [&](const TLDrevenueWithdrawalStatePending &d) {
					entry.status = Data::EarnHistoryEntry::Status::Pending;
				}, [&](const TLDrevenueWithdrawalStateSucceeded &d) {
					entry.successDate = base::unixtime::parse(d.vdate().v);
					entry.successLink = d.vurl().v;
					entry.status = Data::EarnHistoryEntry::Status::Success;
				});
			}, [&](const TLDchatRevenueTransactionTypeRefund &d) {
				entry.type = Data::EarnHistoryEntry::Type::Return;
				entry.date = base::unixtime::parse(d.vrefund_date().v);
				// entry.provider = d.vprovider().v;
			});
		}
		const auto nextToken = token + tlTransactions.size();
		done(Data::EarnHistorySlice{
			.list = std::move(list),
			.total = data.vtotal_count().v,
			.allLoaded = (data.vtotal_count().v <= nextToken),
			.token = Data::EarnHistorySlice::OffsetToken(nextToken),
		});
	}).fail([=] {
		done({});
		_requestId = 0;
	}).send();
}

Data::EarnStatistics ChannelEarnStatistics::data() const {
	return _data;
}

} // namespace Api
