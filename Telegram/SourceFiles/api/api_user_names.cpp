/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_user_names.h"

#include "apiwrap.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "main/main_session.h"

#include "tdb/tdb_tl_scheme.h"

namespace Api {
namespace {

using namespace Tdb;

Data::Usernames SkipSingleNormalUsername(Data::Usernames list) {
	if (list.size() == 1 && list.front().editable && list.front().active) {
		return {};
	}
	return list;
}

#if 0 // mtp
[[nodiscard]] Data::Username UsernameFromTL(const MTPUsername &username) {
	return {
		.username = qs(username.data().vusername()),
		.active = username.data().is_active(),
		.editable = username.data().is_editable(),
	};
}

[[nodiscard]] std::optional<MTPInputUser> BotUserInput(
		not_null<PeerData*> peer) {
	const auto user = peer->asUser();
	return (user && user->botInfo && user->botInfo->canEditInformation)
		? std::make_optional<MTPInputUser>(user->inputUser)
		: std::nullopt;
}
#endif

[[nodiscard]] std::optional<TLint53> BotUserInput(
		not_null<PeerData*> peer) {
	const auto user = peer->asUser();
	return (user && user->botInfo && user->botInfo->canEditInformation)
		? std::make_optional(tl_int53(peerToUser(user->id).bare))
		: std::nullopt;
}

} // namespace

Usernames::Usernames(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance()) {
}

rpl::producer<Data::Usernames> Usernames::loadUsernames(
		not_null<PeerData*> peer) const {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

#if 0 // mtp
		const auto push = [consumer](
				const auto &usernames,
				const auto &username) {
			if (usernames) {
				if (usernames->v.empty()) {
					// Probably will never happen.
					consumer.put_next({});
				} else {
					auto parsed = FromTL(*usernames);
					if ((parsed.size() == 1)
						&& username
						&& (parsed.front().username == qs(*username))) {
						// Probably will never happen.
						consumer.put_next({});
					} else {
						consumer.put_next(std::move(parsed));
					}
				}
			} else {
				consumer.put_next({});
			}
		};

		const auto requestUser = [&](const MTPInputUser &data) {
			_session->api().request(MTPusers_GetUsers(
				MTP_vector<MTPInputUser>(1, data)
			)).done([=](const MTPVector<MTPUser> &result) {
				result.v.front().match([&](const MTPDuser &data) {
					push(data.vusernames(), data.vusername());
					consumer.put_done();
				}, [&](const MTPDuserEmpty&) {
					consumer.put_next({});
					consumer.put_done();
				});
			}).send();
		};
		const auto requestChannel = [&](const MTPInputChannel &data) {
			_session->api().request(MTPchannels_GetChannels(
				MTP_vector<MTPInputChannel>(1, data)
			)).done([=](const MTPmessages_Chats &result) {
				result.match([&](const auto &data) {
					data.vchats().v.front().match([&](const MTPDchannel &c) {
						push(c.vusernames(), c.vusername());
						consumer.put_done();
					}, [&](auto &&) {
						consumer.put_next({});
						consumer.put_done();
					});
				});
			}).send();
		};
		if (peer->isSelf()) {
			requestUser(MTP_inputUserSelf());
		} else if (const auto user = peer->asUser()) {
			requestUser(user->inputUser);
		} else if (const auto channel = peer->asChannel()) {
			requestChannel(channel->inputChannel);
		}
#endif
		const auto requestUser = [&](UserId userId) {
			_session->sender().request(TLgetUser(
				tl_int53(userId.bare)
			)).done([=](const TLDuser &result) {
				consumer.put_next(
					SkipSingleNormalUsername(FromTL(result.vusernames())));
				consumer.put_done();
			}).send();
		};
		const auto requestChannel = [&](ChannelId channelId) {
			_session->sender().request(TLgetSupergroup(
				tl_int53(channelId.bare)
			)).done([=](const TLDsupergroup &result) {
				consumer.put_next(
					SkipSingleNormalUsername(FromTL(result.vusernames())));
				consumer.put_done();
			}).send();
		};
		if (const auto user = peer->asUser()) {
			requestUser(peerToUser(user->id));
		} else if (const auto channel = peer->asChannel()) {
			requestChannel(peerToChannel(channel->id));
		}
		return lifetime;
	};
}

rpl::producer<rpl::no_value, Usernames::Error> Usernames::toggle(
		not_null<PeerData*> peer,
		const QString &username,
		bool active) {
	const auto peerId = peer->id;
	const auto it = _toggleRequests.find(peerId);
	const auto found = (it != end(_toggleRequests));
	auto &entry = (!found
		? _toggleRequests.emplace(
			peerId,
			Entry{ .usernames = { username } }).first
		: it)->second;
	if (ranges::contains(entry.usernames, username)) {
		if (found) {
			return entry.done.events();
		}
	} else {
		entry.usernames.push_back(username);
	}

	const auto pop = [=](Error error) {
		const auto it = _toggleRequests.find(peerId);
		if (it != end(_toggleRequests)) {
			auto &list = it->second.usernames;
			list.erase(ranges::remove(list, username), end(list));
			if (list.empty()) {
				if (error == Error::Unknown) {
					it->second.done.fire_done();
				} else if (error == Error::TooMuch) {
					it->second.done.fire_error_copy(error);
				}
				_toggleRequests.remove(peerId);
			}
		}
	};

	const auto done = [=] {
		pop(Error::Unknown);
	};
	const auto fail = [=](const Tdb::Error &error) {
		const auto type = error.message;
#if 0 // mtp
	const auto fail = [=](const MTP::Error &error) {
		const auto type = error.type();
#endif
		if (type == u"USERNAMES_ACTIVE_TOO_MUCH"_q) {
			pop(Error::TooMuch);
		} else {
			pop(Error::Unknown);
		}
	};

	if (peer->isSelf()) {
		_api.request(TLtoggleUsernameIsActive(
			tl_string(username),
			tl_bool(active)
		)).done(done).fail(fail).send();
#if 0 // mtp
		_api.request(MTPaccount_ToggleUsername(
			MTP_string(username),
			MTP_bool(active)
		)).done(done).fail(fail).send();
#endif
	} else if (const auto channel = peer->asChannel()) {
		_api.request(TLtoggleSupergroupUsernameIsActive(
			tl_int53(peerToChannel(channel->id).bare),
			tl_string(username),
			tl_bool(active)
		)).done(done).fail(fail).send();
#if 0 // mtp
		_api.request(MTPchannels_ToggleUsername(
			channel->inputChannel,
			MTP_string(username),
			MTP_bool(active)
		)).done(done).fail(fail).send();
#endif
	} else if (const auto botUserInput = BotUserInput(peer)) {
		_api.request(TLtoggleBotUsernameIsActive(
			*botUserInput,
			tl_string(username),
			tl_bool(active)
		)).done(done).fail(fail).send();
#if 0 // mtp
		_api.request(MTPbots_ToggleUsername(
			*botUserInput,
			MTP_string(username),
			MTP_bool(active)
		)).done(done).fail(fail).send();
#endif
	} else {
		return rpl::never<rpl::no_value, Error>();
	}
	return entry.done.events();
}

rpl::producer<> Usernames::reorder(
		not_null<PeerData*> peer,
		const std::vector<QString> &usernames) {
	const auto peerId = peer->id;
	const auto it = _reorderRequests.find(peerId);
	if (it != end(_reorderRequests)) {
		_api.request(it->second).cancel();
		_reorderRequests.erase(peerId);
	}

	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

		auto tlUsernames = ranges::views::all(
			usernames
		) | ranges::views::transform([](const QString &username) {
			return tl_string(username);
		}) | ranges::to<QVector<TLstring>>;
#if 0 // mtp
		auto tlUsernames = ranges::views::all(
			usernames
		) | ranges::views::transform([](const QString &username) {
			return MTP_string(username);
		}) | ranges::to<QVector<MTPstring>>;
#endif

		const auto finish = [=] {
			if (_reorderRequests.contains(peerId)) {
				_reorderRequests.erase(peerId);
			}
			consumer.put_done();
		};
		if (usernames.empty()) {
			crl::on_main([=] { consumer.put_done(); });
			return lifetime;
		}

		if (peer->isSelf()) {
			const auto requestId = _api.request(TLreorderActiveUsernames(
				tl_vector<TLstring>(std::move(tlUsernames))
			)).done(finish).fail(finish).send();
#if 0 // mtp
			const auto requestId = _api.request(MTPaccount_ReorderUsernames(
				MTP_vector<MTPstring>(std::move(tlUsernames))
			)).done(finish).fail(finish).send();
#endif
			_reorderRequests.emplace(peerId, requestId);
		} else if (const auto channel = peer->asChannel()) {
			const auto requestId = _api.request(TLreorderSupergroupActiveUsernames(
				tl_int53(peerToChannel(channel->id).bare),
				tl_vector<TLstring>(std::move(tlUsernames))
			)).done(finish).fail(finish).send();
#if 0 // mtp
			const auto requestId = _api.request(MTPchannels_ReorderUsernames(
				channel->inputChannel,
				MTP_vector<MTPstring>(std::move(tlUsernames))
			)).done(finish).fail(finish).send();
#endif
			_reorderRequests.emplace(peerId, requestId);
		} else if (const auto botUserInput = BotUserInput(peer)) {
			const auto requestId = _api.request(TLreorderBotActiveUsernames(
				*botUserInput,
				tl_vector<TLstring>(std::move(tlUsernames))
			)).done(finish).fail(finish).send();
#if 0 // mtp
			const auto requestId = _api.request(MTPbots_ReorderUsernames(
				*botUserInput,
				MTP_vector<MTPstring>(std::move(tlUsernames))
			)).done(finish).fail(finish).send();
#endif
			_reorderRequests.emplace(peerId, requestId);
		}
		return lifetime;
	};
}

#if 0 // mtp
Data::Usernames Usernames::FromTL(const MTPVector<MTPUsername> &usernames) {
	return ranges::views::all(
		usernames.v
	) | ranges::views::transform(UsernameFromTL) | ranges::to_vector;
}
#endif

Data::Usernames Usernames::FromTL(const Tdb::TLusernames *usernames) {
	auto result = Data::Usernames();
	if (const auto data = usernames ? &usernames->data() : nullptr) {
		const auto editable = data->veditable_username().v;
		for (const auto &active : data->vactive_usernames().v) {
			result.push_back({
				.username = active.v,
				.active = true,
				.editable = (active.v == editable),
			});
		}
		for (const auto &inactive : data->vdisabled_usernames().v) {
			result.push_back({
				.username = inactive.v,
				.active = false,
				.editable = (inactive.v == editable),
			});
		}
	}
	return result;
}

void Usernames::requestToCache(not_null<PeerData*> peer) {
	_tinyCache = {};
	if (const auto user = peer->asUser()) {
		if (user->usernames().empty()) {
			return;
		}
	} else if (const auto channel = peer->asChannel()) {
		if (channel->usernames().empty()) {
			return;
		}
	}
	const auto lifetime = std::make_shared<rpl::lifetime>();
	*lifetime = loadUsernames(
		peer
	) | rpl::start_with_next([=, id = peer->id](Data::Usernames usernames) {
		_tinyCache = std::make_pair(id, std::move(usernames));
		lifetime->destroy();
	});
}

Data::Usernames Usernames::cacheFor(PeerId id) {
	return (_tinyCache.first == id) ? _tinyCache.second : Data::Usernames();
}

} // namespace Api
