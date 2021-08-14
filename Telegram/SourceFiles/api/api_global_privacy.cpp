/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_global_privacy.h"

#include "apiwrap.h"
#include "base/const_string.h"
#include "tdb/tdb_option.h"
#include "main/main_session.h"
#include "main/main_app_config.h"

namespace Api {

GlobalPrivacy::GlobalPrivacy(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance()) {
}

bool GlobalPrivacy::apply(const Tdb::TLDupdateOption &option) {
	if (option.vname().v
			== u"can_archive_and_mute_new_chats_from_unknown_users"_q) {
		_showArchiveAndMute = option.vvalue().match([](
				const Tdb::TLDoptionValueBoolean &data) {
			return data.vvalue().v;
		}, [](const auto &) {
			return false;
		});
		return true;
	} else if (option.vname().v == u"is_paid_reaction_anonymous"_q) {
		_paidReactionAnonymous = option.vvalue().match([](
				const Tdb::TLDoptionValueBoolean &data) {
			return data.vvalue().v;
		}, [](const auto &) {
			return false;
		});
		return true;
	}
	return false;
}

#if 0 // goodToRemove
void GlobalPrivacy::reload(Fn<void()> callback) {
	if (callback) {
		_callbacks.push_back(std::move(callback));
	}
	if (_requestId) {
		return;
	}
	_requestId = _api.request(MTPaccount_GetGlobalPrivacySettings(
	)).done([=](const MTPGlobalPrivacySettings &result) {
		_requestId = 0;
		apply(result);
		for (const auto &callback : base::take(_callbacks)) {
			callback();
		}
	}).fail([=] {
		_requestId = 0;
		for (const auto &callback : base::take(_callbacks)) {
			callback();
		}
	}).send();

	_session->appConfig().value(
	) | rpl::start_with_next([=] {
		_showArchiveAndMute = _session->appConfig().get<bool>(
			u"autoarchive_setting_available"_q,
			false);
	}, _session->lifetime());
}
#endif

void GlobalPrivacy::reload(Fn<void()> callback) {
	if (callback) {
		_callbacks.push_back(std::move(callback));
	}
	if (_requesting) {
		return;
	}
	_requesting = true;

	const auto finish = [=] {
		_requesting = false;
		for (const auto &callback : base::take(_callbacks)) {
			callback();
		}
	};

	const auto requestNewChats = [=] {
		_api.request(Tdb::TLgetNewChatPrivacySettings(
		)).done([=](const Tdb::TLDnewChatPrivacySettings &data) {
			_newRequirePremium
				= !data.vallow_new_chats_from_unknown_users().v;
			finish();
		}).fail(finish).send();
	};

	const auto requestReadDate = [=] {
		_api.request(Tdb::TLgetReadDatePrivacySettings(
		)).done([=](const Tdb::TLDreadDatePrivacySettings &data) {
			_hideReadTime = !data.vshow_read_date().v;
			requestNewChats();
		}).fail(requestNewChats).send();
	};

	_api.request(Tdb::TLgetArchiveChatListSettings(
	)).done([=](const Tdb::TLDarchiveChatListSettings &data) {
		_archiveAndMute
			= data.varchive_and_mute_new_chats_from_unknown_users().v;
		requestReadDate();
	}).fail(requestReadDate).send();
}

bool GlobalPrivacy::archiveAndMuteCurrent() const {
	return _archiveAndMute.current();
}

rpl::producer<bool> GlobalPrivacy::archiveAndMute() const {
	return _archiveAndMute.value();
}

UnarchiveOnNewMessage GlobalPrivacy::unarchiveOnNewMessageCurrent() const {
	return _unarchiveOnNewMessage.current();
}

auto GlobalPrivacy::unarchiveOnNewMessage() const
-> rpl::producer<UnarchiveOnNewMessage> {
	return _unarchiveOnNewMessage.value();
}

rpl::producer<bool> GlobalPrivacy::showArchiveAndMute() const {
	using namespace rpl::mappers;

	return rpl::combine(
		archiveAndMute(),
		_showArchiveAndMute.value(),
		_1 || _2);
}

rpl::producer<> GlobalPrivacy::suggestArchiveAndMute() const {
	return _session->appConfig().suggestionRequested(
		u"AUTOARCHIVE_POPULAR"_q);
}

void GlobalPrivacy::dismissArchiveAndMuteSuggestion() {
	_session->appConfig().dismissSuggestion(
		u"AUTOARCHIVE_POPULAR"_q);
}

void GlobalPrivacy::updateHideReadTime(bool hide) {
	update(
		archiveAndMuteCurrent(),
		unarchiveOnNewMessageCurrent(),
		hide,
		newRequirePremiumCurrent());
}

bool GlobalPrivacy::hideReadTimeCurrent() const {
	return _hideReadTime.current();
}

rpl::producer<bool> GlobalPrivacy::hideReadTime() const {
	return _hideReadTime.value();
}

void GlobalPrivacy::updateNewRequirePremium(bool value) {
	update(
		archiveAndMuteCurrent(),
		unarchiveOnNewMessageCurrent(),
		hideReadTimeCurrent(),
		value);
}

bool GlobalPrivacy::newRequirePremiumCurrent() const {
	return _newRequirePremium.current();
}

rpl::producer<bool> GlobalPrivacy::newRequirePremium() const {
	return _newRequirePremium.value();
}

void GlobalPrivacy::loadPaidReactionAnonymous() {
	if (_paidReactionAnonymousLoaded) {
		return;
	}
	_paidReactionAnonymousLoaded = true;
#if 0 // mtp
	_api.request(MTPmessages_GetPaidReactionPrivacy(
	)).done([=](const MTPUpdates &result) {
		_session->api().applyUpdates(result);
	}).send();
#endif
}

void GlobalPrivacy::updatePaidReactionAnonymous(bool value) {
	_paidReactionAnonymous = value;
}

bool GlobalPrivacy::paidReactionAnonymousCurrent() const {
	return _paidReactionAnonymous.current();
}

rpl::producer<bool> GlobalPrivacy::paidReactionAnonymous() const {
	return _paidReactionAnonymous.value();
}

void GlobalPrivacy::updateArchiveAndMute(bool value) {
	update(
		value,
		unarchiveOnNewMessageCurrent(),
		hideReadTimeCurrent(),
		newRequirePremiumCurrent());
}

void GlobalPrivacy::updateUnarchiveOnNewMessage(
		UnarchiveOnNewMessage value) {
	update(
		archiveAndMuteCurrent(),
		value,
		hideReadTimeCurrent(),
		newRequirePremiumCurrent());
}

#if 0 // goodToRemove
void GlobalPrivacy::update(
		bool archiveAndMute,
		UnarchiveOnNewMessage unarchiveOnNewMessage,
		bool hideReadTime,
		bool newRequirePremium) {
	using Flag = MTPDglobalPrivacySettings::Flag;

	_api.request(_requestId).cancel();
	const auto newRequirePremiumAllowed = _session->premium()
		|| _session->appConfig().newRequirePremiumFree();
	const auto flags = Flag()
		| (archiveAndMute
			? Flag::f_archive_and_mute_new_noncontact_peers
			: Flag())
		| (unarchiveOnNewMessage == UnarchiveOnNewMessage::None
			? Flag::f_keep_archived_unmuted
			: Flag())
		| (unarchiveOnNewMessage != UnarchiveOnNewMessage::AnyUnmuted
			? Flag::f_keep_archived_folders
			: Flag())
		| (hideReadTime ? Flag::f_hide_read_marks : Flag())
		| ((newRequirePremium && newRequirePremiumAllowed)
			? Flag::f_new_noncontact_peers_require_premium
			: Flag());
	_requestId = _api.request(MTPaccount_SetGlobalPrivacySettings(
		MTP_globalPrivacySettings(MTP_flags(flags))
	)).done([=](const MTPGlobalPrivacySettings &result) {
		_requestId = 0;
		apply(result);
	}).fail([=](const MTP::Error &error) {
		_requestId = 0;
		if (error.type() == u"PREMIUM_ACCOUNT_REQUIRED"_q) {
			update(archiveAndMute, unarchiveOnNewMessage, hideReadTime, {});
		}
	}).send();
	_archiveAndMute = archiveAndMute;
	_unarchiveOnNewMessage = unarchiveOnNewMessage;
	_hideReadTime = hideReadTime;
	_newRequirePremium = newRequirePremium;
}
#endif

void GlobalPrivacy::update(
		bool archiveAndMute,
		UnarchiveOnNewMessage unarchiveOnNewMessage,
		bool hideReadTime,
		bool newRequirePremium) {
	using Unarchive = UnarchiveOnNewMessage;
	_api.request(Tdb::TLsetArchiveChatListSettings(
		Tdb::tl_archiveChatListSettings(
			Tdb::tl_bool(archiveAndMute),
			Tdb::tl_bool(unarchiveOnNewMessage == Unarchive::None),
			Tdb::tl_bool(unarchiveOnNewMessage != Unarchive::AnyUnmuted))
	)).send();
	_api.request(Tdb::TLsetReadDatePrivacySettings(
		Tdb::tl_readDatePrivacySettings(Tdb::tl_bool(!hideReadTime))
	)).send();
	_api.request(Tdb::TLsetNewChatPrivacySettings(
		Tdb::tl_newChatPrivacySettings(Tdb::tl_bool(!newRequirePremium))
	)).send();
}

#if 0 // goodToRemove
void GlobalPrivacy::apply(const MTPGlobalPrivacySettings &data) {
	data.match([&](const MTPDglobalPrivacySettings &data) {
		_archiveAndMute = data.is_archive_and_mute_new_noncontact_peers();
		_unarchiveOnNewMessage = data.is_keep_archived_unmuted()
			? UnarchiveOnNewMessage::None
			: data.is_keep_archived_folders()
			? UnarchiveOnNewMessage::NotInFoldersUnmuted
			: UnarchiveOnNewMessage::AnyUnmuted;
		_hideReadTime = data.is_hide_read_marks();
		_newRequirePremium = data.is_new_noncontact_peers_require_premium();
	});
}
#endif

} // namespace Api
