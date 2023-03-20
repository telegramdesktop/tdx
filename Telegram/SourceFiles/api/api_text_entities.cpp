/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_text_entities.h"

#include "data/data_document.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "data/stickers/data_stickers_set.h"
#include "main/main_session.h"

#include "tdb/tdb_tl_scheme.h"

namespace Api {
namespace {

using namespace TextUtilities;
using namespace Tdb;

#if 0 // mtp
[[nodiscard]] QString CustomEmojiEntityData(
		const MTPDmessageEntityCustomEmoji &data) {
	return Data::SerializeCustomEmojiId(data.vdocument_id().v);
}

[[nodiscard]] std::optional<MTPMessageEntity> CustomEmojiEntity(
		MTPint offset,
		MTPint length,
		const QString &data) {
	const auto parsed = Data::ParseCustomEmojiData(data);
	if (!parsed) {
		return {};
	}
	return MTP_messageEntityCustomEmoji(
		offset,
		length,
		MTP_long(parsed));
}

[[nodiscard]] std::optional<MTPMessageEntity> MentionNameEntity(
		not_null<Main::Session*> session,
		MTPint offset,
		MTPint length,
		const QString &data) {
	const auto parsed = MentionNameDataToFields(data);
	if (!parsed.userId || parsed.selfId != session->userId().bare) {
		return {};
	}
	return MTP_inputMessageEntityMentionName(
		offset,
		length,
		(parsed.userId == parsed.selfId
			? MTP_inputUserSelf()
			: MTP_inputUser(
				MTP_long(parsed.userId),
				MTP_long(parsed.accessHash))));
}
#endif

} // namespace

#if 0 // mtp
EntitiesInText EntitiesFromMTP(
		Main::Session *session,
		const QVector<MTPMessageEntity> &entities) {
	if (entities.isEmpty()) {
		return {};
	}
	auto result = EntitiesInText();
	result.reserve(entities.size());

	for (const auto &entity : entities) {
		entity.match([&](const MTPDmessageEntityUnknown &d) {
		}, [&](const MTPDmessageEntityMention &d) {
			result.push_back({
				EntityType::Mention,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityHashtag &d) {
			result.push_back({
				EntityType::Hashtag,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityBotCommand &d) {
			result.push_back({
				EntityType::BotCommand,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityUrl &d) {
			result.push_back({
				EntityType::Url,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityEmail &d) {
			result.push_back({
				EntityType::Email,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityBold &d) {
			result.push_back({
				EntityType::Bold,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityItalic &d) {
			result.push_back({
				EntityType::Italic,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityCode &d) {
			result.push_back({
				EntityType::Code,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityPre &d) {
			result.push_back({
				EntityType::Pre,
				d.voffset().v,
				d.vlength().v,
				qs(d.vlanguage()),
			});
		}, [&](const MTPDmessageEntityTextUrl &d) {
			result.push_back({
				EntityType::CustomUrl,
				d.voffset().v,
				d.vlength().v,
				qs(d.vurl()),
			});
		}, [&](const MTPDmessageEntityMentionName &d) {
			if (!session) {
				return;
			}
			const auto userId = UserId(d.vuser_id());
			const auto user = session->data().userLoaded(userId);
			const auto data = MentionNameDataFromFields({
				.selfId = session->userId().bare,
				.userId = userId.bare,
				.accessHash = user ? user->accessHash() : 0,
			});
			result.push_back({
				EntityType::MentionName,
				d.voffset().v,
				d.vlength().v,
				data,
			});
		}, [&](const MTPDinputMessageEntityMentionName &d) {
			if (!session) {
				return;
			}
			const auto data = d.vuser_id().match([&](
					const MTPDinputUserSelf &) {
				return MentionNameDataFromFields({
					.selfId = session->userId().bare,
					.userId = session->userId().bare,
					.accessHash = session->user()->accessHash(),
				});
			}, [&](const MTPDinputUser &data) {
				return MentionNameDataFromFields({
					.selfId = session->userId().bare,
					.userId = UserId(data.vuser_id()).bare,
					.accessHash = data.vaccess_hash().v,
				});
			}, [](const auto &) {
				return QString();
			});
			if (!data.isEmpty()) {
				result.push_back({
					EntityType::MentionName,
					d.voffset().v,
					d.vlength().v,
					data,
				});
			}
		}, [&](const MTPDmessageEntityPhone &d) {
			result.push_back({
				EntityType::Phone,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityCashtag &d) {
			result.push_back({
				EntityType::Cashtag,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityUnderline &d) {
			result.push_back({
				EntityType::Underline,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityStrike &d) {
			result.push_back({
				EntityType::StrikeOut,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityBankCard &d) {
			// Skipping cards. // #TODO entities
		}, [&](const MTPDmessageEntitySpoiler &d) {
			result.push_back({
				EntityType::Spoiler,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const MTPDmessageEntityCustomEmoji &d) {
			result.push_back({
				EntityType::CustomEmoji,
				d.voffset().v,
				d.vlength().v,
				CustomEmojiEntityData(d),
			});
		}, [&](const MTPDmessageEntityBlockquote &d) {
			result.push_back({
				EntityType::Blockquote,
				d.voffset().v,
				d.vlength().v,
				d.is_collapsed() ? u"1"_q : QString(),
			});
		});
	}
	return result;
}

MTPVector<MTPMessageEntity> EntitiesToMTP(
		not_null<Main::Session*> session,
		const EntitiesInText &entities,
		ConvertOption option) {
	auto v = QVector<MTPMessageEntity>();
	v.reserve(entities.size());
	for (const auto &entity : entities) {
		if (entity.length() <= 0) {
			continue;
		}
		if (option == ConvertOption::SkipLocal
			&& entity.type() != EntityType::Bold
			//&& entity.type() != EntityType::Semibold // Not in API.
			&& entity.type() != EntityType::Italic
			&& entity.type() != EntityType::Underline
			&& entity.type() != EntityType::StrikeOut
			&& entity.type() != EntityType::Code // #TODO entities
			&& entity.type() != EntityType::Pre
			&& entity.type() != EntityType::Blockquote
			&& entity.type() != EntityType::Spoiler
			&& entity.type() != EntityType::MentionName
			&& entity.type() != EntityType::CustomUrl
			&& entity.type() != EntityType::CustomEmoji) {
			continue;
		}

		auto offset = MTP_int(entity.offset());
		auto length = MTP_int(entity.length());
		switch (entity.type()) {
		case EntityType::Url: {
			v.push_back(MTP_messageEntityUrl(offset, length));
		} break;
		case EntityType::CustomUrl: {
			v.push_back(
				MTP_messageEntityTextUrl(
					offset,
					length,
					MTP_string(entity.data())));
		} break;
		case EntityType::Email: {
			v.push_back(MTP_messageEntityEmail(offset, length));
		} break;
		case EntityType::Phone: {
			v.push_back(MTP_messageEntityPhone(offset, length));
		} break;
		case EntityType::Hashtag: {
			v.push_back(MTP_messageEntityHashtag(offset, length));
		} break;
		case EntityType::Cashtag: {
			v.push_back(MTP_messageEntityCashtag(offset, length));
		} break;
		case EntityType::Mention: {
			v.push_back(MTP_messageEntityMention(offset, length));
		} break;
		case EntityType::MentionName: {
			const auto valid = MentionNameEntity(
				session,
				offset,
				length,
				entity.data());
			if (valid) {
				v.push_back(*valid);
			}
		} break;
		case EntityType::BotCommand: {
			v.push_back(MTP_messageEntityBotCommand(offset, length));
		} break;
		case EntityType::Bold: {
			v.push_back(MTP_messageEntityBold(offset, length));
		} break;
		case EntityType::Italic: {
			v.push_back(MTP_messageEntityItalic(offset, length));
		} break;
		case EntityType::Underline: {
			v.push_back(MTP_messageEntityUnderline(offset, length));
		} break;
		case EntityType::StrikeOut: {
			v.push_back(MTP_messageEntityStrike(offset, length));
		} break;
		case EntityType::Code: {
			// #TODO entities.
			v.push_back(MTP_messageEntityCode(offset, length));
		} break;
		case EntityType::Pre: {
			v.push_back(
				MTP_messageEntityPre(
					offset,
					length,
					MTP_string(entity.data())));
		} break;
		case EntityType::Blockquote: {
			using Flag = MTPDmessageEntityBlockquote::Flag;
			const auto collapsed = !entity.data().isEmpty();
			v.push_back(
				MTP_messageEntityBlockquote(
					MTP_flags(collapsed ? Flag::f_collapsed : Flag()),
					offset,
					length));
		} break;
		case EntityType::Spoiler: {
			v.push_back(MTP_messageEntitySpoiler(offset, length));
		} break;
		case EntityType::CustomEmoji: {
			const auto valid = CustomEmojiEntity(
				offset,
				length,
				entity.data());
			if (valid) {
				v.push_back(*valid);
			}
		} break;
		}
	}
	return MTP_vector<MTPMessageEntity>(std::move(v));
}
#endif

EntitiesInText EntitiesFromTdb(const QVector<TLtextEntity> &entities) {
	auto result = EntitiesInText();
	if (!entities.isEmpty()) {
		result.reserve(entities.size());
		for (const auto &entity : entities) {
			entity.match([&](const TLDtextEntity &data) {
				const auto offset = data.voffset().v;
				const auto length = data.vlength().v;
				auto additional = QString();
				const auto type = data.vtype().match([&](
						const TLDtextEntityTypeMention &data) {
					return EntityType::Mention;
				}, [&](const TLDtextEntityTypeHashtag &data) {
					return EntityType::Hashtag;
				}, [&](const TLDtextEntityTypeCashtag &data) {
					return EntityType::Cashtag;
				}, [&](const TLDtextEntityTypeBotCommand &data) {
					return EntityType::BotCommand;
				}, [&](const TLDtextEntityTypeUrl &data) {
					return EntityType::Url;
				}, [&](const TLDtextEntityTypeEmailAddress &data) {
					return EntityType::Email;
				}, [&](const TLDtextEntityTypePhoneNumber &data) {
					return EntityType::Invalid;
				}, [&](const TLDtextEntityTypeBankCardNumber &data) {
					return EntityType::Invalid;
				}, [&](const TLDtextEntityTypeBold &data) {
					return EntityType::Bold;
				}, [&](const TLDtextEntityTypeItalic &data) {
					return EntityType::Italic;
				}, [&](const TLDtextEntityTypeUnderline &data) {
					return EntityType::Underline;
				}, [&](const TLDtextEntityTypeStrikethrough &data) {
					return EntityType::StrikeOut;
				}, [&](const TLDtextEntityTypeCode &data) {
					return EntityType::Code;
				}, [&](const TLDtextEntityTypePre &data) {
					return EntityType::Pre;
				}, [&](const TLDtextEntityTypePreCode &data) {
					additional = data.vlanguage().v;
					return EntityType::Pre;
				}, [&](const TLDtextEntityTypeTextUrl &data) {
					additional = data.vurl().v;
					return EntityType::CustomUrl;
				}, [&](const TLDtextEntityTypeMentionName &data) {
					additional = MentionNameDataFromFields({
						.userId = uint64(data.vuser_id().v),
					});
					return EntityType::MentionName;
				}, [&](const TLDtextEntityTypeSpoiler &data) {
					return EntityType::Spoiler;
				}, [&](const TLDtextEntityTypeMediaTimestamp &data) {
					// later entities media timestamp links
					return EntityType::Invalid;
				}, [&](const TLDtextEntityTypeCustomEmoji &data) {
					additional = QString::number(
						uint64(data.vcustom_emoji_id().v));
					return EntityType::CustomEmoji;
				}, [&](const TLDtextEntityTypeBlockQuote &data) {
					return EntityType::Blockquote;
				}, [&](const TLDtextEntityTypeExpandableBlockQuote &data) {
					additional = u"1"_q;
					return EntityType::Blockquote;
				});
				if (type != EntityType::Invalid) {
					result.push_back({ type, offset, length, additional });
				}
			});
		}
	}
	return result;
}

TextWithEntities FormattedTextFromTdb(
		const TLformattedText &text) {
	const auto &formatted = text.data();
	return TextWithEntities{
		formatted.vtext().v,
		Api::EntitiesFromTdb(formatted.ventities().v)
	};
}

QVector<TLtextEntity> EntitiesToTdb(const EntitiesInText &entities) {
	constexpr auto option = ConvertOption::SkipLocal;

	auto v = QVector<TLtextEntity>();
	v.reserve(entities.size());
	for (const auto &entity : entities) {
		if (entity.length() <= 0) {
			continue;
		} else if (option == ConvertOption::SkipLocal
			&& entity.type() != EntityType::Bold
			//&& entity.type() != EntityType::Semibold // Not in API.
			&& entity.type() != EntityType::Italic
			&& entity.type() != EntityType::Underline
			&& entity.type() != EntityType::StrikeOut
			&& entity.type() != EntityType::Code // #TODO entities
			&& entity.type() != EntityType::Pre
			&& entity.type() != EntityType::Blockquote
			&& entity.type() != EntityType::Spoiler
			&& entity.type() != EntityType::MentionName
			&& entity.type() != EntityType::CustomUrl
			&& entity.type() != EntityType::CustomEmoji) {
			continue;
		}

		const auto type = [&]() -> std::optional<TLtextEntityType> {
			switch (entity.type()) {
			case EntityType::Url: return tl_textEntityTypeUrl();
			case EntityType::CustomUrl:
				return tl_textEntityTypeTextUrl(tl_string(entity.data()));
			case EntityType::Email: return tl_textEntityTypeEmailAddress();
			case EntityType::Hashtag: return tl_textEntityTypeHashtag();
			case EntityType::Cashtag: return tl_textEntityTypeCashtag();
			case EntityType::Mention: return tl_textEntityTypeMention();
			case EntityType::MentionName: {
				const auto fields = MentionNameDataToFields(entity.data());
				return fields.userId
					? tl_textEntityTypeMentionName(tl_int53(fields.userId))
					: std::optional<TLtextEntityType>();
			}
			case EntityType::BotCommand:
				return tl_textEntityTypeBotCommand();
			case EntityType::Bold: return tl_textEntityTypeBold();
			case EntityType::Italic: return tl_textEntityTypeItalic();
			case EntityType::Underline: return tl_textEntityTypeUnderline();
			case EntityType::StrikeOut:
				return tl_textEntityTypeStrikethrough();
			case EntityType::Code: return tl_textEntityTypeCode(); // #TODO entities
			case EntityType::Pre:
				return entity.data().isEmpty()
					? tl_textEntityTypePre()
					: tl_textEntityTypePreCode(tl_string(entity.data()));
			case EntityType::Spoiler: return tl_textEntityTypeSpoiler();
			case EntityType::CustomEmoji:
				return tl_textEntityTypeCustomEmoji(
					tl_int64(entity.data().toULongLong()));
			case EntityType::Blockquote:
				return entity.data().isEmpty()
					? tl_textEntityTypeBlockQuote()
					: tl_textEntityTypeExpandableBlockQuote();
			}
			return std::nullopt;
		}();
		if (type) {
			v.push_back(tl_textEntity(
				tl_int32(entity.offset()),
				tl_int32(entity.length()),
				*type));
		}
	}
	return v;
}

TLformattedText FormattedTextToTdb(const TextWithEntities &text) {
	return tl_formattedText(
		tl_string(text.text),
		tl_vector<TLtextEntity>(EntitiesToTdb(text.entities)));
}

} // namespace Api
