/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "dialogs/dialogs_main_list.h"

namespace Tdb {
class TLDupdateSavedMessagesTopic;
class TLDupdateSavedMessagesTopicCount;
} // namespace Tdb

namespace Main {
class Session;
} // namespace Main

namespace Data {

class Session;
class SavedSublist;

class SavedMessages final {
public:
	explicit SavedMessages(not_null<Session*> owner);
	~SavedMessages();

	[[nodiscard]] bool supported() const;

	[[nodiscard]] Session &owner() const;
	[[nodiscard]] Main::Session &session() const;

	[[nodiscard]] not_null<Dialogs::MainList*> chatsList();
	[[nodiscard]] not_null<SavedSublist*> sublist(not_null<PeerData*> peer);

	void loadMore();
	void loadMore(not_null<SavedSublist*> sublist);

#if 0 // mtp
	void apply(const MTPDupdatePinnedSavedDialogs &update);
	void apply(const MTPDupdateSavedDialogPinned &update);
#endif
	void apply(const Tdb::TLDupdateSavedMessagesTopic &update);
	void apply(const Tdb::TLDupdateSavedMessagesTopicCount &update);

	[[nodiscard]] SavedSublistId sublistId(PeerData *savedSublistPeer) const;
	[[nodiscard]] SavedSublistId sublistId(SavedSublist *sublist) const;
	[[nodiscard]] SavedSublist *sublistById(SavedSublistId id) const;

private:
	void loadPinned();
	void apply(const MTPmessages_SavedDialogs &result, bool pinned);

	void sendLoadMore();
	void sendLoadMore(not_null<SavedSublist*> sublist);
	void sendLoadMoreRequests();

	const not_null<Session*> _owner;

	Dialogs::MainList _chatsList;
	base::flat_map<
		not_null<PeerData*>,
		std::unique_ptr<SavedSublist>> _sublists;
	base::flat_map<PeerId, SavedSublistId> _sublistIds;

	base::flat_map<not_null<SavedSublist*>, mtpRequestId> _loadMoreRequests;
	mtpRequestId _loadMoreRequestId = 0;
	mtpRequestId _pinnedRequestId = 0;

#if 0 // mtp
	TimeId _offsetDate = 0;
	MsgId _offsetId = 0;
	PeerData *_offsetPeer = nullptr;
#endif

	SingleQueuedInvokation _loadMore;
	base::flat_set<not_null<SavedSublist*>> _loadMoreSublistsScheduled;
	bool _loadMoreScheduled = false;

	bool _pinnedLoaded = false;
	bool _unsupported = false;

};

} // namespace Data
