/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/flags.h"
#include "data/data_chat_participant_status.h"

namespace tl {
template <typename T>
class vector_type;
} // namespace tl

namespace Tdb {
class TLmessageReplyInfo;
class TLreplyMarkup;
class TLkeyboardButton;
class TLinlineKeyboardButton;
template <typename T>
using TLvector = tl::vector_type<T>;
} // namespace Tdb

namespace Data {
class Session;
} // namespace Data

namespace InlineBots {
enum class PeerType : uint8;
using PeerTypes = base::flags<PeerType>;
} // namespace InlineBots

enum class ReplyMarkupFlag : uint32 {
	None                  = (1U << 0),
	ForceReply            = (1U << 1),
	HasSwitchInlineButton = (1U << 2),
	Inline                = (1U << 3),
	Resize                = (1U << 4),
	SingleUse             = (1U << 5),
	Selective             = (1U << 6),
	IsNull                = (1U << 7),
	OnlyBuyButton         = (1U << 8),
	Persistent            = (1U << 9),
};
inline constexpr bool is_flag_type(ReplyMarkupFlag) { return true; }
using ReplyMarkupFlags = base::flags<ReplyMarkupFlag>;

struct RequestPeerQuery {
	enum class Type : uchar {
		User,
		Group,
		Broadcast,
	};
	enum class Restriction : uchar {
		Any,
		Yes,
		No,
	};

	int maxQuantity = 0;
	Type type = Type::User;
	Restriction userIsBot = Restriction::Any;
	Restriction userIsPremium = Restriction::Any;
	Restriction groupIsForum = Restriction::Any;
	Restriction hasUsername = Restriction::Any;
	bool amCreator = false;
	bool isBotParticipant = false;
	ChatAdminRights myRights = {};
	ChatAdminRights botRights = {};
};
static_assert(std::is_trivially_copy_assignable_v<RequestPeerQuery>);

struct HistoryMessageMarkupButton {
	enum class Type {
		Default,
		Url,
		Callback,
		CallbackWithPassword,
		RequestPhone,
		RequestLocation,
		RequestPoll,
		RequestPeer,
		SwitchInline,
		SwitchInlineSame,
		Game,
		Buy,
		Auth,
		UserProfile,
		WebView,
		SimpleWebView,
		CopyText,
	};

	HistoryMessageMarkupButton(
		Type type,
		const QString &text,
		const QByteArray &data = QByteArray(),
		const QString &forwardText = QString(),
		int64 buttonId = 0);

	static HistoryMessageMarkupButton *Get(
		not_null<Data::Session*> owner,
		FullMsgId itemId,
		int row,
		int column);

	Type type;
	QString text, forwardText;
	QByteArray data;
	int64 buttonId = 0;
	InlineBots::PeerTypes peerTypes = 0;
	mutable mtpRequestId requestId = 0;

};

struct HistoryMessageMarkupData {
	HistoryMessageMarkupData() = default;
#if 0 // mtp
	explicit HistoryMessageMarkupData(const MTPReplyMarkup *data);
#endif

	explicit HistoryMessageMarkupData(const Tdb::TLreplyMarkup *data);

	void fillForwardedData(const HistoryMessageMarkupData &original);

	[[nodiscard]] bool isNull() const;
	[[nodiscard]] bool isTrivial() const;

	using Button = HistoryMessageMarkupButton;
	std::vector<std::vector<Button>> rows;
	ReplyMarkupFlags flags = ReplyMarkupFlag::IsNull;
	QString placeholder;

private:
	struct ButtonData {
		Button::Type type;
		QByteArray data;
		QString forwardText;
		int64 buttonId = 0;
	};

	void fillRows(const QVector<MTPKeyboardButtonRow> &v);

	template <typename TdbButtonType>
	void fillRows(const QVector<Tdb::TLvector<TdbButtonType>> &v);

	[[nodiscard]] ButtonData buttonData(const Tdb::TLkeyboardButton &data);
	[[nodiscard]] ButtonData buttonData(
		const Tdb::TLinlineKeyboardButton &data);

};

struct HistoryMessageRepliesData {
	HistoryMessageRepliesData() = default;
	explicit HistoryMessageRepliesData(const MTPMessageReplies *data);

	explicit HistoryMessageRepliesData(const Tdb::TLmessageReplyInfo *data);

	std::vector<PeerId> recentRepliers;
	ChannelId channelId = 0;
	MsgId readMaxId = 0;
	MsgId maxId = 0;
	int repliesCount = 0;
	bool isNull = true;
	int pts = 0;
};
