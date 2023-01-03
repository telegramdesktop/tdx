/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Tdb {
class TLfoundChatMessages;
} // namespace Tdb

class HistoryItem;
class History;
class PeerData;

namespace Api {

struct FoundMessages {
	int total = -1;
	MessageIdsList messages;
	QString nextToken;
	bool full = false;
};

class MessagesSearch final {
public:
	explicit MessagesSearch(not_null<History*> history);
	~MessagesSearch();

	void searchMessages(const QString &query, PeerData *from);
	void searchMore();

	[[nodiscard]] rpl::producer<FoundMessages> messagesFounds() const;

private:
#if 0 // mtp
	using TLMessages = MTPmessages_Messages;
#endif
	using TLMessages = Tdb::TLfoundChatMessages;
	void searchRequest();
	void searchReceived(
		const TLMessages &result,
		mtpRequestId requestId,
		const QString &nextToken);

	const not_null<History*> _history;

	base::flat_map<QString, TLMessages> _cacheOfStartByToken;

	QString _query;
	PeerData *_from = nullptr;
	MsgId _offsetId;

	int _searchInHistoryRequest = 0; // Not real mtpRequestId.
	mtpRequestId _requestId = 0;

	bool _full = false;

	rpl::event_stream<FoundMessages> _messagesFounds;

};

} // namespace Api
