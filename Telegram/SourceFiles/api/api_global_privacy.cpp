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
#include "main/main_account.h"
#include "main/main_app_config.h"

namespace Api {

GlobalPrivacy::GlobalPrivacy(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->sender()) {
#if 0 // goodToRemove
, _api(&api->instance()) {
#endif
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

	_session->account().appConfig().value(
	) | rpl::start_with_next([=] {
		_showArchiveAndMute = _session->account().appConfig().get<bool>(
			u"autoarchive_setting_available"_q,
			false);
	}, _session->lifetime());
}
#endif

void GlobalPrivacy::reload(Fn<void()> callback) {
	if (callback) {
		_callbacks.push_back(std::move(callback));
	}
	_api.request(Tdb::TLgetArchiveChatListSettings(
	)).done([=](const Tdb::TLDarchiveChatListSettings &data) {
		_archiveAndMute
			= data.varchive_and_mute_new_chats_from_unknown_users().v;
		for (const auto &callback : base::take(_callbacks)) {
			callback();
		}
	}).fail([=] {
		for (const auto &callback : base::take(_callbacks)) {
			callback();
		}
	}).send();
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
	return _session->account().appConfig().suggestionRequested(
		u"AUTOARCHIVE_POPULAR"_q);
}

void GlobalPrivacy::dismissArchiveAndMuteSuggestion() {
	_session->account().appConfig().dismissSuggestion(
		u"AUTOARCHIVE_POPULAR"_q);
}

void GlobalPrivacy::updateArchiveAndMute(bool value) {
	update(value, unarchiveOnNewMessageCurrent());
}

void GlobalPrivacy::updateUnarchiveOnNewMessage(
		UnarchiveOnNewMessage value) {
	update(archiveAndMuteCurrent(), value);
}

#if 0 // goodToRemove
void GlobalPrivacy::update(
		bool archiveAndMute,
		UnarchiveOnNewMessage unarchiveOnNewMessage) {
	using Flag = MTPDglobalPrivacySettings::Flag;

	_api.request(_requestId).cancel();
	const auto flags = Flag()
		| (archiveAndMute
			? Flag::f_archive_and_mute_new_noncontact_peers
			: Flag())
		| (unarchiveOnNewMessage == UnarchiveOnNewMessage::None
			? Flag::f_keep_archived_unmuted
			: Flag())
		| (unarchiveOnNewMessage != UnarchiveOnNewMessage::AnyUnmuted
			? Flag::f_keep_archived_folders
			: Flag());
	_requestId = _api.request(MTPaccount_SetGlobalPrivacySettings(
		MTP_globalPrivacySettings(MTP_flags(flags))
	)).done([=](const MTPGlobalPrivacySettings &result) {
		_requestId = 0;
		apply(result);
	}).fail([=] {
		_requestId = 0;
	}).send();
	_archiveAndMute = archiveAndMute;
	_unarchiveOnNewMessage = unarchiveOnNewMessage;
}
#endif

void GlobalPrivacy::update(
		bool archiveAndMute,
		UnarchiveOnNewMessage unarchiveOnNewMessage) {
	using Unarchive = UnarchiveOnNewMessage;
	_api.request(Tdb::TLsetArchiveChatListSettings(
		Tdb::tl_archiveChatListSettings(
			Tdb::tl_bool(archiveAndMute),
			Tdb::tl_bool(unarchiveOnNewMessage == Unarchive::None),
			Tdb::tl_bool(unarchiveOnNewMessage != Unarchive::AnyUnmuted))
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
	});
}
#endif

} // namespace Api
