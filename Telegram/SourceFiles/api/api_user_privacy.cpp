/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_user_privacy.h"

#include "apiwrap.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "settings/settings_premium.h" // Settings::ShowPremium.

#include "tdb/tdb_account.h"
#include "tdb/tdb_tl_scheme.h"

namespace Api {
namespace {

constexpr auto kMaxRules = 3; // Allow users, disallow users, Option.

using namespace Tdb;

#if 0 // goodToRemove
using TLInputRules = MTPVector<MTPInputPrivacyRule>;
using TLRules = MTPVector<MTPPrivacyRule>;
#endif
using TLInputRules = TLuserPrivacySettingRules;
using TLRules = TLDuserPrivacySettingRules;

TLInputRules RulesToTL(const UserPrivacy::Rule &rule) {
	using Exceptions = UserPrivacy::Exceptions;
	const auto collectInputUsers = [](const Exceptions &exceptions) {
		const auto &peers = exceptions.peers;
		auto result = QVector<TLint53>();
		result.reserve(peers.size());
		for (const auto &peer : peers) {
			if (const auto user = peer->asUser()) {
				result.push_back(
					tl_int53(peerToUser(peer->id).bare));
			}
		}
		return result;
	};
	const auto collectInputChats = [](const Exceptions &exceptions) {
		const auto &peers = exceptions.peers;
		auto result = QVector<TLint53>();
		result.reserve(peers.size());
		for (const auto &peer : peers) {
			if (!peer->isUser()) {
				result.push_back(peerToTdbChat(peer->id));
			}
		}
		return result;
	};

	auto result = QVector<TLuserPrivacySettingRule>();
	result.reserve(kMaxRules);
	if (!rule.ignoreAlways) {
		const auto users = collectInputUsers(rule.always);
		const auto chats = collectInputChats(rule.always);
		if (!users.empty()) {
			result.push_back(
				tl_userPrivacySettingRuleAllowUsers(
					tl_vector<TLint53>(users)));
		}
		if (!chats.empty()) {
			result.push_back(
				tl_userPrivacySettingRuleAllowChatMembers(
					tl_vector<TLint53>(chats)));
		}
		using Option = UserPrivacy::Option;
		if (rule.always.premiums && (rule.option != Option::Everyone)) {
			result.push_back(tl_userPrivacySettingRuleAllowPremiumUsers());
		}
	}
	if (!rule.ignoreNever) {
		const auto users = collectInputUsers(rule.never);
		const auto chats = collectInputChats(rule.never);
		if (!users.empty()) {
			result.push_back(
				tl_userPrivacySettingRuleRestrictUsers(
					tl_vector<TLint53>(users)));
		}
		if (!chats.empty()) {
			result.push_back(
				tl_userPrivacySettingRuleRestrictChatMembers(
					tl_vector<TLint53>(chats)));
		}
	}
	result.push_back([&] {
		switch (rule.option) {
		case UserPrivacy::Option::Everyone:
			return tl_userPrivacySettingRuleAllowAll();
		case UserPrivacy::Option::Contacts:
			return tl_userPrivacySettingRuleAllowContacts();
		//case Option::CloseFriends:
		//	return tl_storyPrivacySettingsCloseFriends();
		case UserPrivacy::Option::Nobody:
			return tl_userPrivacySettingRuleRestrictAll();
		}
		Unexpected("Option value in UserPrivacy CollectResult.");
	}());

	return tl_userPrivacySettingRules(
		tl_vector<TLuserPrivacySettingRule>(std::move(result)));
#if 0 // goodToRemove
	const auto collectInputUsers = [](const Exceptions &exceptions) {
		const auto &peers = exceptions.peers;
		auto result = QVector<MTPInputUser>();
		result.reserve(peers.size());
		for (const auto &peer : peers) {
			if (const auto user = peer->asUser()) {
				result.push_back(user->inputUser);
			}
		}
		return result;
	};
	const auto collectInputChats = [](const Exceptions &exceptions) {
		const auto &peers = exceptions.peers;
		auto result = QVector<MTPlong>();
		result.reserve(peers.size());
		for (const auto &peer : peers) {
			if (!peer->isUser()) {
				result.push_back(peerToBareMTPInt(peer->id));
			}
		}
		return result;
	};

	using Option = UserPrivacy::Option;
	auto result = QVector<MTPInputPrivacyRule>();
	result.reserve(kMaxRules);
	if (!rule.ignoreAlways) {
		const auto users = collectInputUsers(rule.always);
		const auto chats = collectInputChats(rule.always);
		if (!users.empty()) {
			result.push_back(
				MTP_inputPrivacyValueAllowUsers(
					MTP_vector<MTPInputUser>(users)));
		}
		if (!chats.empty()) {
			result.push_back(
				MTP_inputPrivacyValueAllowChatParticipants(
					MTP_vector<MTPlong>(chats)));
		}
		if (rule.always.premiums && (rule.option != Option::Everyone)) {
			result.push_back(MTP_inputPrivacyValueAllowPremium());
		}
	}
	if (!rule.ignoreNever) {
		const auto users = collectInputUsers(rule.never);
		const auto chats = collectInputChats(rule.never);
		if (!users.empty()) {
			result.push_back(
				MTP_inputPrivacyValueDisallowUsers(
					MTP_vector<MTPInputUser>(users)));
		}
		if (!chats.empty()) {
			result.push_back(
				MTP_inputPrivacyValueDisallowChatParticipants(
					MTP_vector<MTPlong>(chats)));
		}
	}
	result.push_back([&] {
		switch (rule.option) {
		case Option::Everyone: return MTP_inputPrivacyValueAllowAll();
		case Option::Contacts: return MTP_inputPrivacyValueAllowContacts();
		case Option::CloseFriends:
			return MTP_inputPrivacyValueAllowCloseFriends();
		case Option::Nobody: return MTP_inputPrivacyValueDisallowAll();
		}
		Unexpected("Option value in Api::UserPrivacy::RulesToTL.");
	}());


	return MTP_vector<MTPInputPrivacyRule>(std::move(result));
#endif
}

UserPrivacy::Rule TLToRules(const TLRules &rules, Data::Session &owner) {
	// This is simplified version of privacy rules interpretation.
	// But it should be fine for all the apps
	// that use the same subset of features.

#if 0 // doLater
	owner.processUsers(data.vusers());
	owner.processChats(data.vchats());
#endif

	using Option = UserPrivacy::Option;
	auto result = UserPrivacy::Rule();
	auto optionSet = false;
	const auto setOption = [&](Option option) {
		if (optionSet) {
			return;
		}
		optionSet = true;
		result.option = option;
	};
	auto &always = result.always.peers;
	auto &never = result.never.peers;
#if 0 // goodToRemove
	const auto feed = [&](const MTPPrivacyRule &rule) {
		rule.match([&](const MTPDprivacyValueAllowAll &) {
			setOption(Option::Everyone);
		}, [&](const MTPDprivacyValueAllowContacts &) {
			setOption(Option::Contacts);
		}, [&](const MTPDprivacyValueAllowCloseFriends &) {
			setOption(Option::CloseFriends);
		}, [&](const MTPDprivacyValueAllowPremium &) {
			result.always.premiums = true;
		}, [&](const MTPDprivacyValueAllowUsers &data) {
			const auto &users = data.vusers().v;
			always.reserve(always.size() + users.size());
			for (const auto &userId : users) {
				const auto user = owner.user(UserId(userId.v));
				if (!base::contains(never, user)
					&& !base::contains(always, user)) {
					always.emplace_back(user);
				}
			}
		}, [&](const MTPDprivacyValueAllowChatParticipants &data) {
			const auto &chats = data.vchats().v;
			always.reserve(always.size() + chats.size());
			for (const auto &chatId : chats) {
				const auto chat = owner.chatLoaded(chatId);
				const auto peer = chat
					? static_cast<PeerData*>(chat)
					: owner.channelLoaded(chatId);
				if (peer
					&& !base::contains(never, peer)
					&& !base::contains(always, peer)) {
					always.emplace_back(peer);
				}
			}
		}, [&](const MTPDprivacyValueDisallowContacts &) {
			// Not supported
		}, [&](const MTPDprivacyValueDisallowAll &) {
			setOption(Option::Nobody);
		}, [&](const MTPDprivacyValueDisallowUsers &data) {
			const auto &users = data.vusers().v;
			never.reserve(never.size() + users.size());
			for (const auto &userId : users) {
				const auto user = owner.user(UserId(userId.v));
				if (!base::contains(always, user)
					&& !base::contains(never, user)) {
					never.emplace_back(user);
				}
			}
		}, [&](const MTPDprivacyValueDisallowChatParticipants &data) {
			const auto &chats = data.vchats().v;
			never.reserve(never.size() + chats.size());
			for (const auto &chatId : chats) {
				const auto chat = owner.chatLoaded(chatId);
				const auto peer = chat
					? static_cast<PeerData*>(chat)
					: owner.channelLoaded(chatId);
				if (peer
					&& !base::contains(always, peer)
					&& !base::contains(never, peer)) {
					never.emplace_back(peer);
				}
			}
		});
	};
#endif
	const auto notNullGet = &not_null<PeerData*>::get;
	const auto feed = [&](const TLuserPrivacySettingRule &rule) {
		rule.match([&](const TLDuserPrivacySettingRuleAllowAll &) {
			setOption(Option::Everyone);
		}, [&](const TLDuserPrivacySettingRuleAllowContacts &) {
			setOption(Option::Contacts);
		}, [&](const TLDuserPrivacySettingRuleAllowPremiumUsers &) {
			result.always.premiums = true;
		}, [&](const TLDuserPrivacySettingRuleAllowUsers &data) {
			const auto &users = data.vuser_ids().v;
			always.reserve(always.size() + users.size());
			for (const auto &userId : users) {
				const auto user = owner.user(UserId(userId.v));
				if (!ranges::contains(never, user)
					&& !ranges::contains(always, user)) {
					always.emplace_back(user);
				}
			}
		}, [&](const TLDuserPrivacySettingRuleAllowChatMembers &data) {
			const auto &chats = data.vchat_ids().v;
			always.reserve(always.size() + chats.size());
			for (const auto &chatId : chats) {
				const auto peerId = peerFromTdbChat(chatId);
				const auto peer = owner.peerLoaded(peerId);
				if (peer
					&& !ranges::contains(never, peer, notNullGet)
					&& !ranges::contains(always, peer, notNullGet)) {
					always.emplace_back(peer);
				}
			}
		}, [&](const TLDuserPrivacySettingRuleRestrictContacts &) {
			// Not supported.
		}, [&](const TLDuserPrivacySettingRuleRestrictAll &) {
			setOption(Option::Nobody);
		}, [&](const TLDuserPrivacySettingRuleRestrictUsers &data) {
			const auto &users = data.vuser_ids().v;
			never.reserve(never.size() + users.size());
			for (const auto &userId : users) {
				const auto user = owner.user(UserId(userId.v));
				if (!ranges::contains(always, user)
					&& !ranges::contains(never, user)) {
					never.emplace_back(user);
				}
			}
		}, [&](const TLDuserPrivacySettingRuleRestrictChatMembers &data) {
			const auto &chats = data.vchat_ids().v;
			never.reserve(never.size() + chats.size());
			for (const auto &chatId : chats) {
				const auto peerId = peerFromTdbChat(chatId);
				const auto peer = owner.peerLoaded(peerId);
				if (peer
					&& !ranges::contains(always, peer, notNullGet)
					&& !ranges::contains(never, peer, notNullGet)) {
					never.emplace_back(peer);
				}
			}
		});
	};
#if 0 // goodToRemove
	for (const auto &rule : rules.v) {
		feed(rule);
	}
	feed(MTP_privacyValueDisallowAll()); // Disallow by default.
#endif
	for (const auto &rule : rules.vrules().v) {
		feed(rule);
	}
	feed(tl_userPrivacySettingRuleRestrictAll()); // Disallow by default.
	return result;
}

#if 0 // goodToRemove
MTPInputPrivacyKey KeyToTL(UserPrivacy::Key key) {
	using Key = UserPrivacy::Key;
	switch (key) {
	case Key::Calls: return MTP_inputPrivacyKeyPhoneCall();
	case Key::Invites: return MTP_inputPrivacyKeyChatInvite();
	case Key::PhoneNumber: return MTP_inputPrivacyKeyPhoneNumber();
	case Key::AddedByPhone: return MTP_inputPrivacyKeyAddedByPhone();
	case Key::LastSeen: return MTP_inputPrivacyKeyStatusTimestamp();
	case Key::CallsPeer2Peer: return MTP_inputPrivacyKeyPhoneP2P();
	case Key::Forwards: return MTP_inputPrivacyKeyForwards();
	case Key::ProfilePhoto: return MTP_inputPrivacyKeyProfilePhoto();
	case Key::Voices: return MTP_inputPrivacyKeyVoiceMessages();
	case Key::About: return MTP_inputPrivacyKeyAbout();
	case Key::Birthday: return MTP_inputPrivacyKeyBirthday();
	}
	Unexpected("Key in Api::UserPrivacy::KetToTL.");
}

std::optional<UserPrivacy::Key> TLToKey(mtpTypeId type) {
	using Key = UserPrivacy::Key;
	switch (type) {
	case mtpc_privacyKeyPhoneNumber:
	case mtpc_inputPrivacyKeyPhoneNumber: return Key::PhoneNumber;
	case mtpc_privacyKeyAddedByPhone:
	case mtpc_inputPrivacyKeyAddedByPhone: return Key::AddedByPhone;
	case mtpc_privacyKeyStatusTimestamp:
	case mtpc_inputPrivacyKeyStatusTimestamp: return Key::LastSeen;
	case mtpc_privacyKeyChatInvite:
	case mtpc_inputPrivacyKeyChatInvite: return Key::Invites;
	case mtpc_privacyKeyPhoneCall:
	case mtpc_inputPrivacyKeyPhoneCall: return Key::Calls;
	case mtpc_privacyKeyPhoneP2P:
	case mtpc_inputPrivacyKeyPhoneP2P: return Key::CallsPeer2Peer;
	case mtpc_privacyKeyForwards:
	case mtpc_inputPrivacyKeyForwards: return Key::Forwards;
	case mtpc_privacyKeyProfilePhoto:
	case mtpc_inputPrivacyKeyProfilePhoto: return Key::ProfilePhoto;
	case mtpc_privacyKeyVoiceMessages:
	case mtpc_inputPrivacyKeyVoiceMessages: return Key::Voices;
	case mtpc_privacyKeyAbout:
	case mtpc_inputPrivacyKeyAbout: return Key::About;
	case mtpc_privacyKeyBirthday:
	case mtpc_inputPrivacyKeyBirthday: return Key::Birthday;
	}
	return std::nullopt;
}
#endif

TLuserPrivacySetting KeyToTL(UserPrivacy::Key key) {
	using Key = UserPrivacy::Key;
	switch (key) {
	case Key::LastSeen: return tl_userPrivacySettingShowStatus();
	case Key::ProfilePhoto: return tl_userPrivacySettingShowProfilePhoto();
	case Key::Forwards:
		return tl_userPrivacySettingShowLinkInForwardedMessages();
	case Key::PhoneNumber: return tl_userPrivacySettingShowPhoneNumber();
	case Key::Invites: return tl_userPrivacySettingAllowChatInvites();
	case Key::Calls: return tl_userPrivacySettingAllowCalls();
	case Key::CallsPeer2Peer:
		return tl_userPrivacySettingAllowPeerToPeerCalls();
	case Key::AddedByPhone:
		return tl_userPrivacySettingAllowFindingByPhoneNumber();
	case Key::Voices:
		return tl_userPrivacySettingAllowPrivateVoiceAndVideoNoteMessages();
	case Key::About: return tl_userPrivacySettingShowBio();
	case Key::Birthday: return tl_userPrivacySettingShowBirthdate();
	}
	Unexpected("Key in Api::UserPrivacy::KetToTL.");
}

UserPrivacy::Key TLToKey(const TLuserPrivacySetting &setting) {
	return setting.match([](const TLDuserPrivacySettingShowStatus &data) {
		return UserPrivacy::Key::LastSeen;
	}, [](const TLDuserPrivacySettingShowProfilePhoto &data) {
		return UserPrivacy::Key::ProfilePhoto;
	}, [](const TLDuserPrivacySettingShowLinkInForwardedMessages &data) {
		return UserPrivacy::Key::Forwards;
	}, [](const TLDuserPrivacySettingShowPhoneNumber &data) {
		return UserPrivacy::Key::PhoneNumber;
	}, [](const TLDuserPrivacySettingAllowChatInvites &data) {
		return UserPrivacy::Key::Invites;
	}, [](const TLDuserPrivacySettingAllowCalls &data) {
		return UserPrivacy::Key::Calls;
	}, [](const TLDuserPrivacySettingAllowPeerToPeerCalls &data) {
		return UserPrivacy::Key::CallsPeer2Peer;
	}, [](const TLDuserPrivacySettingAllowFindingByPhoneNumber &data) {
		return UserPrivacy::Key::AddedByPhone;
	}, [](TLDuserPrivacySettingAllowPrivateVoiceAndVideoNoteMessages) {
		return UserPrivacy::Key::Voices;
	}, [](const TLDuserPrivacySettingShowBio &) {
		return UserPrivacy::Key::About;
	}, [](const TLDuserPrivacySettingShowBirthdate &) {
		return UserPrivacy::Key::Birthday;
	});
}

} // namespace

UserPrivacy::UserPrivacy(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance()) {
}

#if 0 // goodToRemove
void UserPrivacy::save(
		Key key,
		const UserPrivacy::Rule &rule) {
	const auto tlKey = KeyToTL(key);
	const auto keyTypeId = tlKey.type();
	const auto it = _privacySaveRequests.find(keyTypeId);
	if (it != _privacySaveRequests.cend()) {
		_api.request(it->second).cancel();
		_privacySaveRequests.erase(it);
	}

	const auto requestId = _api.request(MTPaccount_SetPrivacy(
		tlKey,
		RulesToTL(rule)
	)).done([=](const MTPaccount_PrivacyRules &result) {
		result.match([&](const MTPDaccount_privacyRules &data) {
			_session->data().processUsers(data.vusers());
			_session->data().processChats(data.vchats());
			_privacySaveRequests.remove(keyTypeId);
			apply(keyTypeId, data.vrules(), true);
		});
	}).fail([=](const MTP::Error &error) {
		const auto message = error.type();
		if (message == u"PREMIUM_ACCOUNT_REQUIRED"_q) {
			Settings::ShowPremium(_session, QString());
		}
		_privacySaveRequests.remove(keyTypeId);
	}).send();

	_privacySaveRequests.emplace(keyTypeId, requestId);
}
#endif

void UserPrivacy::save(
		Key key,
		const UserPrivacy::Rule &rule) {
	_api.request(TLsetUserPrivacySettingRules(
		KeyToTL(key),
		RulesToTL(rule)
	)).send();
}

rpl::producer<UserPrivacy::Rule> UserPrivacy::value(UserPrivacy::Key key) {
	using Result = std::optional<Rule>;
	auto mapUpdate = [=](const TLupdate &update) {
		return update.match([&](
				const TLDupdateUserPrivacySettingRules &rules) -> Result {
			if (TLToKey(rules.vsetting()) != key) {
				return std::nullopt;
			}
			return TLToRules(rules.vrules().data(), _session->data());
		}, [](const auto &) -> Result {
			return std::nullopt;
		});
	};

	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();
		_api.request(TLgetUserPrivacySettingRules(
			KeyToTL(key)
		)).done([=](const TLRules &data) {
			consumer.put_next(std::move(TLToRules(data, _session->data())));
		}).send();

		_session->tdb().updates(
		) | rpl::map(
			mapUpdate
		) | rpl::start_with_next([=](Result r) {
			if (r) {
				consumer.put_next(base::take(*r));
			}
		}, lifetime);
		return lifetime;
	};
}

#if 0 // goodToRemove
void UserPrivacy::apply(
		mtpTypeId type,
		const TLRules &rules,
		bool allLoaded) {
	if (const auto key = TLToKey(type)) {
		if (!allLoaded) {
			reload(*key);
			return;
		}
		pushPrivacy(*key, rules);
		if ((*key) == Key::LastSeen) {
			_session->api().updatePrivacyLastSeens();
		}
	}
}

void UserPrivacy::reload(Key key) {
	if (_privacyRequestIds.contains(key)) {
		return;
	}
	const auto requestId = _api.request(MTPaccount_GetPrivacy(
		KeyToTL(key)
	)).done([=](const MTPaccount_PrivacyRules &result) {
		_privacyRequestIds.erase(key);
		result.match([&](const MTPDaccount_privacyRules &data) {
			_session->data().processUsers(data.vusers());
			_session->data().processChats(data.vchats());
			pushPrivacy(key, data.vrules());
		});
	}).fail([=] {
		_privacyRequestIds.erase(key);
	}).send();
	_privacyRequestIds.emplace(key, requestId);
}

void UserPrivacy::pushPrivacy(Key key, const TLRules &rules) {
	const auto &saved
		= (_privacyValues[key] = TLToRules(rules, _session->data()));
	const auto i = _privacyChanges.find(key);
	if (i != end(_privacyChanges)) {
		i->second.fire_copy(saved);
	}
}

auto UserPrivacy::value(Key key) -> rpl::producer<UserPrivacy::Rule> {
	if (const auto i = _privacyValues.find(key); i != end(_privacyValues)) {
		return _privacyChanges[key].events_starting_with_copy(i->second);
	} else {
		return _privacyChanges[key].events();
	}
}
#endif

} // namespace Api
