/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_chat_participant_status.h"

#include "base/unixtime.h"
#include "boxes/peers/edit_peer_permissions_box.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "data/data_forum_topic.h"
#include "data/data_peer_values.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/chat/attach/attach_prepare.h"

#include "tdb/tdb_tl_scheme.h"

#if 0 // goodToRemove
namespace {

[[nodiscard]] ChatAdminRights ChatAdminRightsFlags(
		const MTPChatAdminRights &rights) {
	return rights.match([](const MTPDchatAdminRights &data) {
		return ChatAdminRights::from_raw(int32(data.vflags().v));
	});
}

[[nodiscard]] ChatRestrictions ChatBannedRightsFlags(
		const MTPChatBannedRights &rights) {
	return rights.match([](const MTPDchatBannedRights &data) {
		return ChatRestrictions::from_raw(int32(data.vflags().v));
	});
}

[[nodiscard]] TimeId ChatBannedRightsUntilDate(
		const MTPChatBannedRights &rights) {
	return rights.match([](const MTPDchatBannedRights &data) {
		return data.vuntil_date().v;
	});
}

} // namespace

ChatAdminRightsInfo::ChatAdminRightsInfo(const MTPChatAdminRights &rights)
: flags(ChatAdminRightsFlags(rights)) {
}

ChatRestrictionsInfo::ChatRestrictionsInfo(const MTPChatBannedRights &rights)
: flags(ChatBannedRightsFlags(rights))
, until(ChatBannedRightsUntilDate(rights)) {
}
#endif

ChatAdminRightsInfo::ChatAdminRightsInfo(
		const Tdb::TLchatMemberStatus &status) {
	const auto empty = ChatAdminRights(0);
	status.match([&](const Tdb::TLDchatMemberStatusAdministrator &data) {
		using Flag = ChatAdminRight;
		const auto &rights = data.vrights().data();
		flags = empty
			| (rights.vcan_change_info().v ? Flag::ChangeInfo : empty)
			| (rights.vcan_post_messages().v ? Flag::PostMessages : empty)
			| (rights.vcan_edit_messages().v ? Flag::EditMessages : empty)
			| (rights.vcan_delete_messages().v ? Flag::DeleteMessages : empty)
			| (rights.vcan_restrict_members().v ? Flag::BanUsers : empty)
			| (rights.vcan_invite_users().v ? Flag::InviteUsers : empty)
			| (rights.vcan_pin_messages().v ? Flag::PinMessages : empty)
			| (rights.vcan_promote_members().v ? Flag::AddAdmins : empty)
			| (rights.vis_anonymous().v ? Flag::Anonymous : empty)
			| (rights.vcan_manage_video_chats().v ? Flag::ManageCall : empty)
			| (rights.vcan_manage_chat().v ? Flag::Other : empty);
	}, [&](const Tdb::TLDchatMemberStatusCreator &data) {
		_creator = true;
		flags = ChatAdminRight::ChangeInfo
			| ChatAdminRight::PostMessages
			| ChatAdminRight::EditMessages
			| ChatAdminRight::DeleteMessages
			| ChatAdminRight::BanUsers
			| ChatAdminRight::InviteUsers
			| ChatAdminRight::PinMessages
			| ChatAdminRight::AddAdmins
			| (data.vis_anonymous().v ? ChatAdminRight::Anonymous : empty)
			| ChatAdminRight::ManageCall
			| ChatAdminRight::Other;
	}, [](const auto &) {
	});
}

Tdb::TLchatMemberStatus ChatAdminRightsInfo::ToTL(
		const ChatAdminRightsInfo &rights,
		const QString &rank) {
	return Tdb::tl_chatMemberStatusAdministrator(
		Tdb::tl_string(rank),
		Tdb::tl_bool(true),
		Tdb::tl_chatAdministratorRights(
			Tdb::tl_bool(rights.flags & ChatAdminRight::Other),
			Tdb::tl_bool(rights.flags & ChatAdminRight::ChangeInfo),
			Tdb::tl_bool(rights.flags & ChatAdminRight::PostMessages),
			Tdb::tl_bool(rights.flags & ChatAdminRight::EditMessages),
			Tdb::tl_bool(rights.flags & ChatAdminRight::DeleteMessages),
			Tdb::tl_bool(rights.flags & ChatAdminRight::InviteUsers),
			Tdb::tl_bool(rights.flags & ChatAdminRight::BanUsers),
			Tdb::tl_bool(rights.flags & ChatAdminRight::PinMessages),
			Tdb::tl_bool(rights.flags & ChatAdminRight::AddAdmins),
			Tdb::tl_bool(rights.flags & ChatAdminRight::ManageCall),
			Tdb::tl_bool(rights.flags & ChatAdminRight::Anonymous)));
}

ChatRestrictionsInfo::ChatRestrictionsInfo(
		const Tdb::TLchatMemberStatus &status) {
	status.match([&](const Tdb::TLDchatMemberStatusRestricted &status) {
		until = status.vrestricted_until_date().v;
		const auto &data = status.vpermissions().data();
		const auto empty = ChatRestrictions(0);
		using Flag = ChatRestriction;
		flags = empty
			| (data.vcan_send_messages().v ? Flag::SendMessages : empty)
			| (data.vcan_send_media_messages().v ? Flag::SendMedia : empty)
			| (data.vcan_send_other_messages().v
				? (Flag::SendStickers
					| Flag::SendGifs
					| Flag::SendInline
					| Flag::SendGames)
				: empty)
			| (data.vcan_add_web_page_previews().v ? Flag::EmbedLinks : empty)
			| (data.vcan_send_polls().v ? Flag::SendPolls : empty)
			| (data.vcan_change_info().v ? Flag::ChangeInfo : empty)
			| (data.vcan_invite_users().v ? Flag::InviteUsers : empty)
			| (data.vcan_pin_messages().v ? Flag::PinMessages : empty);
	}, [&](const Tdb::TLDchatMemberStatusBanned &data) {
#if 0 // doLater Check if it's an user.
#endif
		until = data.vbanned_until_date().v;
		flags |= ChatRestriction::ViewMessages;
	}, [](const auto &) {
	});
}

Tdb::TLchatPermissions ChatRestrictionsInfo::ToTLPermissions(
		const ChatRestrictions &flags) {
	return Tdb::tl_chatPermissions(
		Tdb::tl_bool(!(flags & ChatRestriction::SendMessages)),
		Tdb::tl_bool(!(flags & ChatRestriction::SendMedia)),
		Tdb::tl_bool(!(flags & ChatRestriction::SendPolls)),
		Tdb::tl_bool((!(flags & ChatRestriction::SendStickers))
			|| (!(flags & ChatRestriction::SendGifs))
			|| (!(flags & ChatRestriction::SendInline))
			|| (!(flags & ChatRestriction::SendGames))),
		Tdb::tl_bool(!(flags & ChatRestriction::EmbedLinks)),
		Tdb::tl_bool(!(flags & ChatRestriction::ChangeInfo)),
		Tdb::tl_bool(!(flags & ChatRestriction::InviteUsers)),
		Tdb::tl_bool(!(flags & ChatRestriction::PinMessages)));
}

Tdb::TLchatMemberStatus ChatRestrictionsInfo::ToTL(
		const ChatRestrictionsInfo &rights) {
	const auto kicked = rights.flags & ChatRestriction::ViewMessages;
	return kicked
		? Tdb::tl_chatMemberStatusBanned(Tdb::tl_int32(rights.until))
		: Tdb::tl_chatMemberStatusRestricted(
			Tdb::tl_bool(true),
			Tdb::tl_int32(rights.until),
			ToTLPermissions(rights.flags));

}

namespace Data {

std::vector<ChatRestrictions> ListOfRestrictions(
		RestrictionsSetOptions options) {
	auto labels = RestrictionLabels(options);
	return ranges::views::all(labels)
		| ranges::views::transform(&RestrictionLabel::flags)
		| ranges::to_vector;
}

ChatRestrictions AllSendRestrictions() {
	constexpr auto result = [] {
		auto result = ChatRestrictions();
		for (const auto right : AllSendRestrictionsList()) {
			result |= right;
		}
		return result;
	}();
	return result;
}

ChatRestrictions FilesSendRestrictions() {
	constexpr auto result = [] {
		auto result = ChatRestrictions();
		for (const auto right : FilesSendRestrictionsList()) {
			result |= right;
		}
		return result;
	}();
	return result;
}

ChatRestrictions TabbedPanelSendRestrictions() {
	constexpr auto result = [] {
		auto result = ChatRestrictions();
		for (const auto right : TabbedPanelSendRestrictionsList()) {
			result |= right;
		}
		return result;
	}();
	return result;
}

// Duplicated in CanSendAnyOfValue().
bool CanSendAnyOf(
		not_null<const Thread*> thread,
		ChatRestrictions rights,
		bool forbidInForums) {
	const auto peer = thread->peer();
	const auto topic = thread->asTopic();
	return CanSendAnyOf(peer, rights, forbidInForums && !topic)
		&& (!topic || !topic->closed() || topic->canToggleClosed());
}

// Duplicated in CanSendAnyOfValue().
bool CanSendAnyOf(
		not_null<const PeerData*> peer,
		ChatRestrictions rights,
		bool forbidInForums) {
	if (const auto user = peer->asUser()) {
		if (user->isInaccessible()
			|| user->isRepliesChat()
			|| user->isVerifyCodes()) {
			return false;
		} else if (user->meRequiresPremiumToWrite()
			&& !user->session().premium()) {
			return false;
		} else if (rights
			& ~(ChatRestriction::SendVoiceMessages
				| ChatRestriction::SendVideoMessages
				| ChatRestriction::SendPolls)) {
			return true;
		}
		for (const auto right : {
			ChatRestriction::SendVoiceMessages,
			ChatRestriction::SendVideoMessages,
			ChatRestriction::SendPolls,
		}) {
			if ((rights & right) && !user->amRestricted(right)) {
				return true;
			}
		}
		return false;
	} else if (const auto chat = peer->asChat()) {
		if (!chat->amIn()) {
			return false;
		}
		for (const auto right : AllSendRestrictionsList()) {
			if ((rights & right) && !chat->amRestricted(right)) {
				return true;
			}
		}
		return false;
	} else if (const auto channel = peer->asChannel()) {
		using Flag = ChannelDataFlag;
		const auto allowed = channel->amIn()
			|| ((channel->flags() & Flag::HasLink)
				&& !(channel->flags() & Flag::JoinToWrite));
		if (!allowed || (forbidInForums && channel->isForum())) {
			return false;
		} else if (channel->canPostMessages()) {
			return true;
		} else if (channel->isBroadcast()) {
			return false;
		}
		for (const auto right : AllSendRestrictionsList()) {
			if ((rights & right) && !channel->amRestricted(right)) {
				return true;
			}
		}
		return false;
	}
	Unexpected("Peer type in CanSendAnyOf.");
}

std::optional<QString> RestrictionError(
		not_null<PeerData*> peer,
		ChatRestriction restriction) {
	using Flag = ChatRestriction;
	if (const auto restricted = peer->amRestricted(restriction)) {
		if (const auto user = peer->asUser()) {
			if (user->meRequiresPremiumToWrite()
				&& !user->session().premium()) {
				return tr::lng_restricted_send_non_premium(
					tr::now,
					lt_user,
					user->shortName());
			}
			const auto result = (restriction == Flag::SendVoiceMessages)
				? tr::lng_restricted_send_voice_messages(
					tr::now,
					lt_user,
					user->name())
				: (restriction == Flag::SendVideoMessages)
				? tr::lng_restricted_send_video_messages(
					tr::now,
					lt_user,
					user->name())
				: (restriction == Flag::SendPolls)
				? u"can't send polls :("_q
				: (restriction == Flag::PinMessages)
				? u"can't pin :("_q
				: std::optional<QString>();

			Ensures(result.has_value());
			return result;
		}
		const auto all = restricted.isWithEveryone();
		const auto channel = peer->asChannel();
		if (!all && channel) {
			auto restrictedUntil = channel->restrictedUntil();
			if (restrictedUntil > 0
				&& !ChannelData::IsRestrictedForever(restrictedUntil)) {
				auto restrictedUntilDateTime = base::unixtime::parse(
					channel->restrictedUntil());
				auto date = QLocale().toString(
					restrictedUntilDateTime.date(),
					QLocale::ShortFormat);
				auto time = QLocale().toString(
					restrictedUntilDateTime.time(),
					QLocale::ShortFormat);

				switch (restriction) {
				case Flag::SendPolls:
					return tr::lng_restricted_send_polls_until(
						tr::now, lt_date, date, lt_time, time);
				case Flag::SendOther:
					return tr::lng_restricted_send_message_until(
						tr::now, lt_date, date, lt_time, time);
				case Flag::SendPhotos:
					return tr::lng_restricted_send_photos_until(
						tr::now, lt_date, date, lt_time, time);
				case Flag::SendVideos:
					return tr::lng_restricted_send_videos_until(
						tr::now, lt_date, date, lt_time, time);
				case Flag::SendMusic:
					return tr::lng_restricted_send_music_until(
						tr::now, lt_date, date, lt_time, time);
				case Flag::SendFiles:
					return tr::lng_restricted_send_files_until(
						tr::now, lt_date, date, lt_time, time);
				case Flag::SendVideoMessages:
					return tr::lng_restricted_send_video_messages_until(
						tr::now, lt_date, date, lt_time, time);
				case Flag::SendVoiceMessages:
					return tr::lng_restricted_send_voice_messages_until(
						tr::now, lt_date, date, lt_time, time);
				case Flag::SendStickers:
					return tr::lng_restricted_send_stickers_until(
						tr::now, lt_date, date, lt_time, time);
				case Flag::SendGifs:
					return tr::lng_restricted_send_gifs_until(
						tr::now, lt_date, date, lt_time, time);
				case Flag::SendInline:
				case Flag::SendGames:
					return tr::lng_restricted_send_inline_until(
						tr::now, lt_date, date, lt_time, time);
				}
				Unexpected("Restriction in Data::RestrictionErrorKey.");
			}
		}
		switch (restriction) {
		case Flag::SendPolls:
			return all
				? tr::lng_restricted_send_polls_all(tr::now)
				: tr::lng_restricted_send_polls(tr::now);
		case Flag::SendOther:
			return all
				? tr::lng_restricted_send_message_all(tr::now)
				: tr::lng_restricted_send_message(tr::now);
		case Flag::SendPhotos:
			return all
				? tr::lng_restricted_send_photos_all(tr::now)
				: tr::lng_restricted_send_photos(tr::now);
		case Flag::SendVideos:
			return all
				? tr::lng_restricted_send_videos_all(tr::now)
				: tr::lng_restricted_send_videos(tr::now);
		case Flag::SendMusic:
			return all
				? tr::lng_restricted_send_music_all(tr::now)
				: tr::lng_restricted_send_music(tr::now);
		case Flag::SendFiles:
			return all
				? tr::lng_restricted_send_files_all(tr::now)
				: tr::lng_restricted_send_files(tr::now);
		case Flag::SendVideoMessages:
			return all
				? tr::lng_restricted_send_video_messages_all(tr::now)
				: tr::lng_restricted_send_video_messages_group(tr::now);
		case Flag::SendVoiceMessages:
			return all
				? tr::lng_restricted_send_voice_messages_all(tr::now)
				: tr::lng_restricted_send_voice_messages_group(tr::now);
		case Flag::SendStickers:
			return all
				? tr::lng_restricted_send_stickers_all(tr::now)
				: tr::lng_restricted_send_stickers(tr::now);
		case Flag::SendGifs:
			return all
				? tr::lng_restricted_send_gifs_all(tr::now)
				: tr::lng_restricted_send_gifs(tr::now);
		case Flag::SendInline:
		case Flag::SendGames:
			return all
				? tr::lng_restricted_send_inline_all(tr::now)
				: tr::lng_restricted_send_inline(tr::now);
		}
		Unexpected("Restriction in Data::RestrictionErrorKey.");
	}
	return std::nullopt;
}

std::optional<QString> AnyFileRestrictionError(not_null<PeerData*> peer) {
	using Restriction = ChatRestriction;
	for (const auto right : FilesSendRestrictionsList()) {
		if (!RestrictionError(peer, right)) {
			return {};
		}
	}
	return RestrictionError(peer, Restriction::SendFiles);
}

std::optional<QString> FileRestrictionError(
		not_null<PeerData*> peer,
		const Ui::PreparedList &list,
		std::optional<bool> compress) {
	const auto slowmode = peer->slowmodeApplied();
	if (slowmode) {
		if (!list.canBeSentInSlowmode()) {
			return tr::lng_slowmode_no_many(tr::now);
		} else if (list.files.size() > 1 && list.hasSticker()) {
			if (compress == false) {
				return tr::lng_slowmode_no_many(tr::now);
			} else {
				compress = true;
			}
		}
	}
	for (const auto &file : list.files) {
		if (const auto error = FileRestrictionError(peer, file, compress)) {
			return error;
		}
	}
	return {};
}

std::optional<QString> FileRestrictionError(
		not_null<PeerData*> peer,
		const Ui::PreparedFile &file,
		std::optional<bool> compress) {
	using Type = Ui::PreparedFile::Type;
	using Restriction = ChatRestriction;
	const auto stickers = RestrictionError(peer, Restriction::SendStickers);
	const auto gifs = RestrictionError(peer, Restriction::SendGifs);
	const auto photos = RestrictionError(peer, Restriction::SendPhotos);
	const auto videos = RestrictionError(peer, Restriction::SendVideos);
	const auto music = RestrictionError(peer, Restriction::SendMusic);
	const auto files = RestrictionError(peer, Restriction::SendFiles);
	if (!stickers && !gifs && !photos && !videos && !music && !files) {
		return {};
	}
	switch (file.type) {
	case Type::Photo:
		if (compress == true && photos) {
			return photos;
		} else if (const auto other = file.isSticker() ? stickers : files) {
			if ((compress == false || photos) && other) {
				return (file.isSticker() || !photos) ? other : photos;
			}
		}
		break;
	case Type::Video:
		if (const auto error = file.isGifv() ? gifs : videos) {
			return error;
		}
		break;
	case Type::Music:
		if (music) {
			return music;
		}
		break;
	case Type::File:
		if (files) {
			return files;
		}
		break;
	}
	return {};
}

} // namespace Data
