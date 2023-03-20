/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/history_item_reply_markup.h"

#include "data/data_session.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "inline_bots/bot_attach_web_view.h"

#include "tdb/tdb_tl_scheme.h"

namespace {

using namespace Tdb;

[[nodiscard]] InlineBots::PeerTypes PeerTypesFromMTP(
		const MTPvector<MTPInlineQueryPeerType> &types) {
	using namespace InlineBots;
	auto result = PeerTypes(0);
	for (const auto &type : types.v) {
		result |= type.match([&](const MTPDinlineQueryPeerTypePM &data) {
			return PeerType::User;
		}, [&](const MTPDinlineQueryPeerTypeChat &data) {
			return PeerType::Group;
		}, [&](const MTPDinlineQueryPeerTypeMegagroup &data) {
			return PeerType::Group;
		}, [&](const MTPDinlineQueryPeerTypeBroadcast &data) {
			return PeerType::Broadcast;
		}, [&](const MTPDinlineQueryPeerTypeBotPM &data) {
			return PeerType::Bot;
		}, [&](const MTPDinlineQueryPeerTypeSameBotPM &data) {
			return PeerType();
		});
	}
	return result;
}

[[nodiscard]] RequestPeerQuery RequestPeerQueryFromTL(
		const TLDkeyboardButtonTypeRequestUsers &data) {
	const auto restriction = [](const TLbool &is, const TLbool &no) {
		using Restriction = RequestPeerQuery::Restriction;
		return is.v
			? Restriction::Yes
			: no.v
			? Restriction::No
			: Restriction::Any;
	};
	return {
		.maxQuantity = data.vmax_quantity().v,
		.type = RequestPeerQuery::Type::User,
		.userIsBot = restriction(
			data.vuser_is_bot(),
			data.vrestrict_user_is_bot()),
		.userIsPremium = restriction(
			data.vuser_is_premium(),
			data.vrestrict_user_is_premium()),
	};
}

[[nodiscard]] RequestPeerQuery RequestPeerQueryFromTL(
		const TLDkeyboardButtonTypeRequestChat &data) {
	using Type = RequestPeerQuery::Type;
	const auto restriction = [](const TLbool &is, const TLbool &no) {
		using Restriction = RequestPeerQuery::Restriction;
		return is.v
			? Restriction::Yes
			: no.v
			? Restriction::No
			: Restriction::Any;
	};
	const auto rights = [](const TLchatAdministratorRights *value) {
		return value
			? AdminRightsFromChatAdministratorRights(*value)
			: ChatAdminRights();
	};
	return {
		.type = (data.vchat_is_channel().v ? Type::Broadcast : Type::Group),
		.groupIsForum = restriction(
			data.vchat_is_forum(),
			data.vrestrict_chat_is_forum()),
		.hasUsername = restriction(
			data.vchat_has_username(),
			data.vrestrict_chat_has_username()),
		.amCreator = data.vchat_is_created().v,
		.isBotParticipant = data.vbot_is_member().v,
		.myRights = rights(data.vuser_administrator_rights()),
		.botRights = rights(data.vbot_administrator_rights()),
	};
}

#if 0 // mtp
[[nodiscard]] RequestPeerQuery RequestPeerQueryFromTL(
		const MTPDkeyboardButtonRequestPeer &query) {
	using Type = RequestPeerQuery::Type;
	using Restriction = RequestPeerQuery::Restriction;
	auto result = RequestPeerQuery();
	result.maxQuantity = query.vmax_quantity().v;
	const auto restriction = [](const MTPBool *value) {
		return !value
			? Restriction::Any
			: mtpIsTrue(*value)
			? Restriction::Yes
			: Restriction::No;
	};
	const auto rights = [](const MTPChatAdminRights *value) {
		return value ? ChatAdminRightsInfo(*value).flags : ChatAdminRights();
	};
	query.vpeer_type().match([&](const MTPDrequestPeerTypeUser &data) {
		result.type = Type::User;
		result.userIsBot = restriction(data.vbot());
		result.userIsPremium = restriction(data.vpremium());
	}, [&](const MTPDrequestPeerTypeChat &data) {
		result.type = Type::Group;
		result.amCreator = data.is_creator();
		result.isBotParticipant = data.is_bot_participant();
		result.groupIsForum = restriction(data.vforum());
		result.hasUsername = restriction(data.vhas_username());
		result.myRights = rights(data.vuser_admin_rights());
		result.botRights = rights(data.vbot_admin_rights());
	}, [&](const MTPDrequestPeerTypeBroadcast &data) {
		result.type = Type::Broadcast;
		result.amCreator = data.is_creator();
		result.hasUsername = restriction(data.vhas_username());
		result.myRights = rights(data.vuser_admin_rights());
		result.botRights = rights(data.vbot_admin_rights());
	});
	return result;
}
#endif

} // namespace

HistoryMessageMarkupButton::HistoryMessageMarkupButton(
	Type type,
	const QString &text,
	const QByteArray &data,
	const QString &forwardText,
	int64 buttonId)
: type(type)
, text(text)
, forwardText(forwardText)
, data(data)
, buttonId(buttonId) {
}

HistoryMessageMarkupButton *HistoryMessageMarkupButton::Get(
		not_null<Data::Session*> owner,
		FullMsgId itemId,
		int row,
		int column) {
	if (const auto item = owner->message(itemId)) {
		if (const auto markup = item->Get<HistoryMessageReplyMarkup>()) {
			if (row < markup->data.rows.size()) {
				auto &buttons = markup->data.rows[row];
				if (column < buttons.size()) {
					return &buttons[column];
				}
			}
		}
	}
	return nullptr;
}

#if 0 // mtp
void HistoryMessageMarkupData::fillRows(
		const QVector<MTPKeyboardButtonRow> &list) {
	rows.clear();
	if (list.isEmpty()) {
		return;
	}

	using Type = Button::Type;
	rows.reserve(list.size());
	for (const auto &row : list) {
		row.match([&](const MTPDkeyboardButtonRow &data) {
			auto row = std::vector<Button>();
			row.reserve(data.vbuttons().v.size());
			for (const auto &button : data.vbuttons().v) {
				button.match([&](const MTPDkeyboardButton &data) {
					row.emplace_back(Type::Default, qs(data.vtext()));
				}, [&](const MTPDkeyboardButtonCallback &data) {
					row.emplace_back(
						(data.is_requires_password()
							? Type::CallbackWithPassword
							: Type::Callback),
						qs(data.vtext()),
						qba(data.vdata()));
				}, [&](const MTPDkeyboardButtonRequestGeoLocation &data) {
					row.emplace_back(Type::RequestLocation, qs(data.vtext()));
				}, [&](const MTPDkeyboardButtonRequestPhone &data) {
					row.emplace_back(Type::RequestPhone, qs(data.vtext()));
				}, [&](const MTPDkeyboardButtonRequestPeer &data) {
					const auto query = RequestPeerQueryFromTL(data);
					row.emplace_back(
						Type::RequestPeer,
						qs(data.vtext()),
						QByteArray(
							reinterpret_cast<const char*>(&query),
							sizeof(query)),
						QString(),
						int64(data.vbutton_id().v));
				}, [&](const MTPDkeyboardButtonUrl &data) {
					row.emplace_back(
						Type::Url,
						qs(data.vtext()),
						qba(data.vurl()));
				}, [&](const MTPDkeyboardButtonSwitchInline &data) {
					const auto type = data.is_same_peer()
						? Type::SwitchInlineSame
						: Type::SwitchInline;
					row.emplace_back(type, qs(data.vtext()), qba(data.vquery()));
					if (type == Type::SwitchInline) {
						// Optimization flag.
						// Fast check on all new messages if there is a switch button to auto-click it.
						flags |= ReplyMarkupFlag::HasSwitchInlineButton;
						if (const auto types = data.vpeer_types()) {
							row.back().peerTypes = PeerTypesFromMTP(*types);
						}
					}
				}, [&](const MTPDkeyboardButtonGame &data) {
					row.emplace_back(Type::Game, qs(data.vtext()));
				}, [&](const MTPDkeyboardButtonBuy &data) {
					row.emplace_back(Type::Buy, qs(data.vtext()));
				}, [&](const MTPDkeyboardButtonUrlAuth &data) {
					row.emplace_back(
						Type::Auth,
						qs(data.vtext()),
						qba(data.vurl()),
						qs(data.vfwd_text().value_or_empty()),
						data.vbutton_id().v);
				}, [&](const MTPDkeyboardButtonRequestPoll &data) {
					const auto quiz = [&] {
						if (!data.vquiz()) {
							return QByteArray();
						}
						return data.vquiz()->match([&](const MTPDboolTrue&) {
							return QByteArray(1, 1);
						}, [&](const MTPDboolFalse&) {
							return QByteArray(1, 0);
						});
					}();
					row.emplace_back(
						Type::RequestPoll,
						qs(data.vtext()),
						quiz);
				}, [&](const MTPDkeyboardButtonUserProfile &data) {
					row.emplace_back(
						Type::UserProfile,
						qs(data.vtext()),
						QByteArray::number(data.vuser_id().v));
				}, [&](const MTPDinputKeyboardButtonUrlAuth &data) {
					LOG(("API Error: inputKeyboardButtonUrlAuth."));
					// Should not get those for the users.
				}, [&](const MTPDinputKeyboardButtonUserProfile &data) {
					LOG(("API Error: inputKeyboardButtonUserProfile."));
					// Should not get those for the users.
				}, [&](const MTPDkeyboardButtonWebView &data) {
					row.emplace_back(
						Type::WebView,
						qs(data.vtext()),
						data.vurl().v);
				}, [&](const MTPDkeyboardButtonSimpleWebView &data) {
					row.emplace_back(
						Type::SimpleWebView,
						qs(data.vtext()),
						data.vurl().v);
				}, [&](const MTPDkeyboardButtonCopy &data) {
					row.emplace_back(
						Type::CopyText,
						qs(data.vtext()),
						data.vcopy_text().v);
				}, [&](const MTPDinputKeyboardButtonRequestPeer &data) {
					LOG(("API Error: inputKeyboardButtonRequestPeer."));
					// Should not get those for the users.
				});
			}
			if (!row.empty()) {
				rows.push_back(std::move(row));
			}
		});
	}
	if (rows.size() == 1
		&& rows.front().size() == 1
		&& rows.front().front().type == Type::Buy) {
		flags |= ReplyMarkupFlag::OnlyBuyButton;
	}
}
#endif

template <typename TdbButtonType>
void HistoryMessageMarkupData::fillRows(
		const QVector<TLvector<TdbButtonType>> &list) {
	rows.clear();
	if (list.isEmpty()) {
		return;
	}

	rows.reserve(list.size());
	for (const auto &buttons : list) {
		auto row = std::vector<Button>();
		row.reserve(buttons.v.size());
		for (const auto &button : buttons.v) {
			const auto text = button.data().vtext().v;
			const auto data = buttonData(button);
			if (data.type == Button::Type::SwitchInline) {
				// Optimization flag.
				// Fast check on all new messages if there is a switch button
				// to auto-click it.
				flags |= ReplyMarkupFlag::HasSwitchInlineButton;
			}
			row.emplace_back(
				data.type,
				text,
				data.data,
				data.forwardText,
				data.buttonId);
		}
		if (!row.empty()) {
			rows.push_back(std::move(row));
		}
	}
}

auto HistoryMessageMarkupData::buttonData(const TLkeyboardButton &button)
-> ButtonData {
	using Type = Button::Type;
	return button.data().vtype().match([&](
		const TLDkeyboardButtonTypeText &data) {
		return ButtonData{ Type::Default };
	}, [&](const TLDkeyboardButtonTypeRequestPoll &data) {
		const auto quiz = data.vforce_regular().v
			? QByteArray(1, 0)
			: data.vforce_quiz().v
			? QByteArray(1, 1)
			: QByteArray();
		return ButtonData{ Type::RequestPoll, quiz };
	}, [&](const TLDkeyboardButtonTypeRequestLocation &data) {
		return ButtonData{ Type::RequestLocation };
	}, [&](const TLDkeyboardButtonTypeRequestPhoneNumber &data) {
		return ButtonData{ Type::RequestPhone };
	}, [&](const TLDkeyboardButtonTypeWebApp &data) {
		return ButtonData{ Type::SimpleWebView, data.vurl().v.toUtf8()};
	}, [&](const TLDkeyboardButtonTypeRequestUsers &data) {
		const auto query = RequestPeerQueryFromTL(data);
		return ButtonData{
			Type::RequestPeer,
			QByteArray(
				reinterpret_cast<const char*>(&query),
				sizeof(query)),
			QString(),
			int64(data.vid().v)
		};
	}, [&](const TLDkeyboardButtonTypeRequestChat &data) {
		const auto query = RequestPeerQueryFromTL(data);
		return ButtonData{
			Type::RequestPeer,
			QByteArray(
				reinterpret_cast<const char*>(&query),
				sizeof(query)),
			QString(),
			int64(data.vid().v)
		};
	});
}

auto HistoryMessageMarkupData::buttonData(
	const TLinlineKeyboardButton &button)
-> ButtonData {
	using Type = Button::Type;
	return button.data().vtype().match([&](
			const TLDinlineKeyboardButtonTypeUrl &data) {
		return ButtonData{ Type::Url, data.vurl().v.toUtf8() };
	}, [&](const TLDinlineKeyboardButtonTypeLoginUrl &data) {
		return ButtonData{
			Type::Auth,
			data.vurl().v.toUtf8(),
			data.vforward_text().v,
			data.vid().v,
		};
	}, [&](const TLDinlineKeyboardButtonTypeCallback &data) {
		return ButtonData{ Type::Callback, data.vdata().v };
	}, [&](const TLDinlineKeyboardButtonTypeCallbackWithPassword &data) {
		return ButtonData{ Type::CallbackWithPassword, data.vdata().v };
	}, [&](const TLDinlineKeyboardButtonTypeCallbackGame &data) {
		return ButtonData{ Type::Game };
	}, [&](const TLDinlineKeyboardButtonTypeSwitchInline &data) {
		const auto type = data.vin_current_chat().v
			? Type::SwitchInlineSame
			: Type::SwitchInline;
		return ButtonData{ type, data.vquery().v.toUtf8() };
	}, [&](const TLDinlineKeyboardButtonTypeBuy &data) {
		return ButtonData{ Type::Buy };
	}, [&](const TLDinlineKeyboardButtonTypeUser &data) {
		return ButtonData{
			Type::UserProfile,
			QByteArray::number(data.vuser_id().v),
		};
	}, [&](const TLDinlineKeyboardButtonTypeWebApp &data) {
		return ButtonData{ Type::WebView, data.vurl().v.toUtf8() };
	}, [&](const TLDinlineKeyboardButtonTypeCopyText &data) {
		return ButtonData{ Type::CopyText, data.vtext().v.toUtf8() };
	});
}

#if 0 // mtp
HistoryMessageMarkupData::HistoryMessageMarkupData(
		const MTPReplyMarkup *data) {
	if (!data) {
		return;
	}
	using Flag = ReplyMarkupFlag;
	data->match([&](const MTPDreplyKeyboardMarkup &data) {
		flags = (data.is_resize() ? Flag::Resize : Flag())
			| (data.is_selective() ? Flag::Selective : Flag())
			| (data.is_single_use() ? Flag::SingleUse : Flag())
			| (data.is_persistent() ? Flag::Persistent : Flag());
		placeholder = qs(data.vplaceholder().value_or_empty());
		fillRows(data.vrows().v);
	}, [&](const MTPDreplyInlineMarkup &data) {
		flags = Flag::Inline;
		placeholder = QString();
		fillRows(data.vrows().v);
	}, [&](const MTPDreplyKeyboardHide &data) {
		flags = Flag::None
			| (data.is_selective() ? Flag::Selective : Flag());
		placeholder = QString();
	}, [&](const MTPDreplyKeyboardForceReply &data) {
		flags = Flag::ForceReply
			| (data.is_selective() ? Flag::Selective : Flag())
			| (data.is_single_use() ? Flag::SingleUse : Flag());
		placeholder = qs(data.vplaceholder().value_or_empty());
	});
}
#endif

HistoryMessageMarkupData::HistoryMessageMarkupData(
		const TLreplyMarkup *data) {
	if (!data) {
		return;
	}
	using Flag = ReplyMarkupFlag;
	data->match([&](const TLDreplyMarkupShowKeyboard &data) {
		flags = (data.vresize_keyboard().v ? Flag::Resize : Flag())
			| (data.vis_personal().v ? Flag::Selective : Flag())
			| (data.vone_time().v ? Flag::SingleUse : Flag())
			| (data.vis_persistent().v ? Flag::Persistent : Flag());
		placeholder = data.vinput_field_placeholder().v;
		fillRows(data.vrows().v);
	}, [&](const TLDreplyMarkupInlineKeyboard &data) {
		flags = Flag::Inline;
		placeholder = QString();
		fillRows(data.vrows().v);
	}, [&](const TLDreplyMarkupRemoveKeyboard &data) {
		flags = Flag::None
			| (data.vis_personal().v ? Flag::Selective : Flag());
		placeholder = QString();
	}, [&](const TLDreplyMarkupForceReply &data) {
		flags = Flag::ForceReply
			| (data.vis_personal().v ? Flag::Selective : Flag());
		placeholder = data.vinput_field_placeholder().v;
	});
}

void HistoryMessageMarkupData::fillForwardedData(
		const HistoryMessageMarkupData &original) {
	Expects(isNull());
	Expects(!original.isNull());

	flags = original.flags;
	placeholder = original.placeholder;

	rows.reserve(original.rows.size());
	using Type = HistoryMessageMarkupButton::Type;
	for (const auto &existing : original.rows) {
		auto row = std::vector<Button>();
		row.reserve(existing.size());
		for (const auto &button : existing) {
			const auto newType = (button.type != Type::SwitchInlineSame)
				? button.type
				: Type::SwitchInline;
			const auto text = button.forwardText.isEmpty()
				? button.text
				: button.forwardText;
			row.emplace_back(
				newType,
				text,
				button.data,
				QString(),
				button.buttonId);
		}
		if (!row.empty()) {
			rows.push_back(std::move(row));
		}
	}
}

bool HistoryMessageMarkupData::isNull() const {
	if (flags & ReplyMarkupFlag::IsNull) {
		Assert(isTrivial());
		return true;
	}
	return false;
}

bool HistoryMessageMarkupData::isTrivial() const {
	return rows.empty()
		&& placeholder.isEmpty()
		&& !(flags & ~ReplyMarkupFlag::IsNull);
}

#if 0 // mtp
HistoryMessageRepliesData::HistoryMessageRepliesData(
		const MTPMessageReplies *data) {
	if (!data) {
		return;
	}
	const auto &fields = data->c_messageReplies();
	if (const auto list = fields.vrecent_repliers()) {
		recentRepliers.reserve(list->v.size());
		for (const auto &id : list->v) {
			recentRepliers.push_back(peerFromMTP(id));
		}
	}
	repliesCount = fields.vreplies().v;
	channelId = ChannelId(fields.vchannel_id().value_or_empty());
	readMaxId = fields.vread_max_id().value_or_empty();
	maxId = fields.vmax_id().value_or_empty();
	isNull = false;
	pts = fields.vreplies_pts().v;
}
#endif

HistoryMessageRepliesData::HistoryMessageRepliesData(
		const TLmessageReplyInfo *data) {
	if (!data) {
		return;
	}
	const auto &fields = data->data();
	const auto &repliers = fields.vrecent_replier_ids().v;
	recentRepliers.reserve(repliers.size());
	for (const auto &sender : repliers) {
		recentRepliers.push_back(peerFromSender(sender));
	}
	repliesCount = fields.vreply_count().v;
	readMaxId = fields.vlast_read_inbox_message_id().v;
	maxId = fields.vlast_message_id().v;
	isNull = false;
}
