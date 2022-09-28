/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_message_reaction_id.h"

#include "data/stickers/data_custom_emoji.h"

#include "tdb/tdb_tl_scheme.h"

namespace Data {
namespace {

using namespace Tdb;

} // namespace

QString ReactionEntityData(const ReactionId &id) {
	if (id.empty()) {
		return {};
	} else if (const auto custom = id.custom()) {
		return SerializeCustomEmojiId(custom);
	}
	return u"default:"_q + id.emoji();
}

#if 0 // mtp
ReactionId ReactionFromMTP(const MTPReaction &reaction) {
	return reaction.match([](MTPDreactionEmpty) {
		return ReactionId{ QString() };
	}, [](const MTPDreactionEmoji &data) {
		return ReactionId{ qs(data.vemoticon()) };
	}, [](const MTPDreactionCustomEmoji &data) {
		return ReactionId{ DocumentId(data.vdocument_id().v) };
	});
}

MTPReaction ReactionToMTP(ReactionId id) {
	if (const auto custom = id.custom()) {
		return MTP_reactionCustomEmoji(MTP_long(custom));
	}
	const auto emoji = id.emoji();
	return emoji.isEmpty()
		? MTP_reactionEmpty()
		: MTP_reactionEmoji(MTP_string(emoji));
}
#endif

ReactionId ReactionFromTL(const TLreactionType &reaction) {
	return reaction.match([&](const TLDreactionTypeEmoji &data) {
		return ReactionId{ data.vemoji().v };
	}, [&](const TLDreactionTypeCustomEmoji &data) {
		return ReactionId{ DocumentId(data.vcustom_emoji_id().v) };
	});
}

TLreactionType ReactionToTL(ReactionId id) {
	const auto custom = id.custom();
	return custom
		? tl_reactionTypeCustomEmoji(tl_int64(custom))
		: tl_reactionTypeEmoji(tl_string(id.emoji()));
}

std::optional<TLreactionType> ReactionToMaybeTL(ReactionId id) {
	return id.empty() ? std::optional<TLreactionType>() : ReactionToTL(id);
}

} // namespace Data
