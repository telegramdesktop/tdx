/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/history_view_fake_items.h"

#include "base/unixtime.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"

#include "tdb/tdb_tl_scheme.h"

namespace HistoryView {
namespace {

using namespace Tdb;

} // namespace

AdminLog::OwnedItem GenerateItem(
		not_null<HistoryView::ElementDelegate*> delegate,
		not_null<History*> history,
		PeerId from,
		FullMsgId replyTo,
		const QString &text,
		EffectId effectId) {
	Expects(history->peer->isUser());

	const auto item = history->addNewLocalMessage({
		.id = history->nextNonHistoryEntryId(),
		.flags = (MessageFlag::FakeHistoryItem
			| MessageFlag::HasFromId
			| MessageFlag::HasReplyInfo),
		.from = from,
		.replyTo = FullReplyTo{.messageId = replyTo },
		.date = base::unixtime::now(),
		.effectId = effectId,
	}, TextWithEntities{ .text = text }, MTP_messageMediaEmpty());

	return AdminLog::OwnedItem(delegate, item);
}

PeerId GenerateUser(not_null<History*> history, const QString &name) {
	Expects(history->peer->isUser());

	const auto peerId = Data::FakePeerIdForJustName(name);
	history->owner().processUser(tl_user(
		peerToTdbChat(peerId),
		tl_string(name),
		TLstring(), // last_name_
		TLstring(), // username_
		TLstring(), // phone_number_
		tl_userStatusEmpty(), // status_
		null, // profile_photo_
		tl_int32(Data::DecideColorIndex(peerId)),
		tl_int64(0), // background_custom_emoji_id_
		tl_int32(0), // profile_accent_color_id_
		tl_int64(0), // profile_background_custom_emoji_id_
		null, // emoji_status_
		tl_bool(false), // is_contact_
		tl_bool(false), // is_mutual_contact_
		tl_bool(false), // is_close_friend_
		tl_bool(false), // is_verified_
		tl_bool(false), // is_premium_
		tl_bool(false), // is_support_
		TLstring(), // restriction_reason_
		tl_bool(false), // is_scam_
		tl_bool(false), // is_fake_
		tl_bool(false), // has_active_stories_
		tl_bool(false), // has_unread_active_stories_
		tl_bool(false), // restricts_new_chats
		tl_bool(true), // have_access_
		tl_userTypeRegular(), // type_
		TLstring(), // language_code_
		tl_bool(false))); // added_to_attachment_menu_
#if 0 // goodToRemove
	history->owner().processUser(MTP_user(
		MTP_flags(MTPDuser::Flag::f_first_name | MTPDuser::Flag::f_min),
		peerToBareMTPInt(peerId),
		MTP_long(0),
		MTP_string(name),
		MTPstring(), // last name
		MTPstring(), // username
		MTPstring(), // phone
		MTPUserProfilePhoto(), // profile photo
		MTPUserStatus(), // status
		MTP_int(0), // bot info version
		MTPVector<MTPRestrictionReason>(), // restrictions
		MTPstring(), // bot placeholder
		MTPstring(), // lang code
		MTPEmojiStatus(),
		MTPVector<MTPUsername>(),
		MTPint(), // stories_max_id
		MTPPeerColor(), // color
		MTPPeerColor(), // profile_color
		MTPint())); // bot_active_users
#endif
	return peerId;
}

} // namespace HistoryView
