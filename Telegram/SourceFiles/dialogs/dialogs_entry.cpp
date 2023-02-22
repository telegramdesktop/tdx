/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/dialogs_entry.h"

#include "dialogs/dialogs_key.h"
#include "dialogs/dialogs_indexed_list.h"
#include "data/data_changes.h"
#include "data/data_session.h"
#include "data/data_folder.h"
#include "data/data_forum_topic.h"
#include "data/data_chat_filters.h"
#include "data/data_saved_sublist.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "mainwidget.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "ui/text/text_options.h"
#include "ui/ui_utility.h"
#include "history/history.h"
#include "history/history_item.h"
#include "styles/style_dialogs.h" // st::dialogsTextWidthMin

namespace Dialogs {
namespace {

auto DialogsPosToTopShift = 0;

uint64 DialogPosFromDate(TimeId date) {
	if (!date) {
		return 0;
	}
	return (uint64(date) << 32) | (++DialogsPosToTopShift);
}

uint64 FixedOnTopDialogPos(int index) {
	return 0xFFFFFFFFFFFF000FULL - index;
}

uint64 PinnedDialogPos(int pinnedIndex) {
	return 0xFFFFFFFF000000FFULL - pinnedIndex;
}

} // namespace

BadgesState BadgesForUnread(
		const UnreadState &state,
		CountInBadge count,
		IncludeInBadge include) {
	const auto countMessages = (count == CountInBadge::Messages)
		|| ((count == CountInBadge::Default)
			&& Core::App().settings().countUnreadMessages());
	const auto counterFull = state.marks
		+ (countMessages ? state.messages : state.chats);
	const auto counterMuted = state.marksMuted
		+ (countMessages ? state.messagesMuted : state.chatsMuted);
	const auto unreadMuted = (counterFull <= counterMuted);

	const auto includeMuted = (include == IncludeInBadge::All)
		|| (include == IncludeInBadge::UnmutedOrAll && unreadMuted)
		|| ((include == IncludeInBadge::Default)
			&& Core::App().settings().includeMutedCounter());

	const auto marks = state.marks - (includeMuted ? 0 : state.marksMuted);
	const auto counter = counterFull - (includeMuted ? 0 : counterMuted);
	const auto mark = (counter == 1) && (marks == 1);
	return {
		.unreadCounter = mark ? 0 : counter,
		.unread = (counter > 0),
		.unreadMuted = includeMuted && (counter <= counterMuted),
		.mention = (state.mentions > 0),
		.reaction = (state.reactions > 0),
		.reactionMuted = (state.reactions <= state.reactionsMuted),
	};
}

Entry::Entry(not_null<Data::Session*> owner, Type type)
: _owner(owner)
, _flags((type == Type::History)
	? (Flag::IsThread | Flag::IsHistory)
	: (type == Type::ForumTopic)
	? Flag::IsThread
	: (type == Type::SavedSublist)
	? Flag::IsSavedSublist
	: Flag(0)) {
}

Entry::~Entry() = default;

Data::Session &Entry::owner() const {
	return *_owner;
}

Main::Session &Entry::session() const {
	return _owner->session();
}

History *Entry::asHistory() {
	return (_flags & Flag::IsHistory)
		? static_cast<History*>(this)
		: nullptr;
}

Data::Forum *Entry::asForum() {
	return (_flags & Flag::IsHistory)
		? static_cast<History*>(this)->peer->forum()
		: nullptr;
}

Data::Folder *Entry::asFolder() {
	return (_flags & (Flag::IsThread | Flag::IsSavedSublist))
		? nullptr
		: static_cast<Data::Folder*>(this);
}

Data::Thread *Entry::asThread() {
	return (_flags & Flag::IsThread)
		? static_cast<Data::Thread*>(this)
		: nullptr;
}

Data::ForumTopic *Entry::asTopic() {
	return ((_flags & Flag::IsThread) && !(_flags & Flag::IsHistory))
		? static_cast<Data::ForumTopic*>(this)
		: nullptr;
}

Data::SavedSublist *Entry::asSublist() {
	return (_flags & Flag::IsSavedSublist)
		? static_cast<Data::SavedSublist*>(this)
		: nullptr;
}

const History *Entry::asHistory() const {
	return const_cast<Entry*>(this)->asHistory();
}

const Data::Forum *Entry::asForum() const {
	return const_cast<Entry*>(this)->asForum();
}

const Data::Folder *Entry::asFolder() const {
	return const_cast<Entry*>(this)->asFolder();
}

const Data::Thread *Entry::asThread() const {
	return const_cast<Entry*>(this)->asThread();
}

const Data::ForumTopic *Entry::asTopic() const {
	return const_cast<Entry*>(this)->asTopic();
}

const Data::SavedSublist *Entry::asSublist() const {
	return const_cast<Entry*>(this)->asSublist();
}

void Entry::pinnedIndexChanged(FilterId filterId, int was, int now) {
	if (!filterId && session().supportMode()) {
		// Force reorder in support mode.
		_sortKeyInChatList = 0;
	}
	refreshChatListSortPositionFromTdb(
		filterId,
		tdbOrderInChatList(filterId));
	updateChatListSortPosition();
	updateChatListEntry();
	if ((was != 0) != (now != 0)) {
		changedChatListPinHook();
	}
}

void Entry::cachePinnedIndex(FilterId filterId, int index) {
	const auto i = _pinnedIndex.find(filterId);
	const auto was = (i != end(_pinnedIndex)) ? i->second : 0;
	if (index == was) {
		return;
	}
	if (!index) {
		_pinnedIndex.erase(i);
	} else if (!was) {
		_pinnedIndex.emplace(filterId, index);
	} else {
		i->second = index;
	}
	pinnedIndexChanged(filterId, was, index);
}

#if 0 // mtp
bool Entry::needUpdateInChatList() const {
	return inChatList() || shouldBeInChatList();
}
#endif

void Entry::updateChatListSortPosition() {
#if 0 // mtp
	if (session().supportMode()
		&& _sortKeyInChatList != 0
		&& session().settings().supportFixChatsOrder()) {
		updateChatListEntry();
		return;
	}
	_sortKeyByDate = DialogPosFromDate(adjustedChatListTimeId());
	const auto fixedIndex = fixedOnTopIndex();
	_sortKeyInChatList = fixedIndex
		? FixedOnTopDialogPos(fixedIndex)
		: computeSortPosition(0);
	if (needUpdateInChatList()) {
		setChatListExistence(true);
	} else {
		_sortKeyInChatList = _sortKeyByDate = 0;
	}
#endif
	if (const auto folder = asFolder()) {
		const auto order = folder->chatsList()->empty()
			? 0
			: FixedOnTopDialogPos(kArchiveFixOnTopIndex);
		updateChatListSortPosition(FilterId(), order, false);
	} else if (const auto topic = asTopic()) {
		// tdlib won't be needed when tdlib manages topics
		auto sortKeyByDate = DialogPosFromDate(
			topic->adjustedChatListTimeId());
		const auto pinnedIndex = lookupPinnedIndex(FilterId());
		if (inChatList()
			|| (pinnedIndex != 0)
			|| !topic->lastMessageKnown()
			|| (topic->lastMessage() != nullptr)) {
			const auto fixedIndex = fixedOnTopIndex();
			_sortKeyInChatList = fixedIndex
				? FixedOnTopDialogPos(fixedIndex)
				: pinnedIndex
				? PinnedDialogPos(pinnedIndex)
				: sortKeyByDate;
		} else {
			_sortKeyInChatList = 0;
		}
		if (_sortKeyInChatList) {
			owner().refreshChatListEntry(this, FilterId());
			updateChatListEntry();
		} else {
			owner().removeChatListEntry(this, FilterId());
		}
	}
}

void Entry::updateChatListSortPosition(
		FilterId filterId,
		uint64 order,
		bool pinned) {
	const auto pinnedUnchanged = [&] {
		return (pinned == isPinnedDialog(filterId));
	};
	if (filterId) {
		const auto i = _tdbOrderInFilterMap.find(filterId);
		const auto unchanged = order
			? (i != end(_tdbOrderInFilterMap) && i->second == order)
			: (i == end(_tdbOrderInFilterMap));
		if (unchanged && pinnedUnchanged()) {
			return;
		} else if (!order) {
			_tdbOrderInFilterMap.erase(i);
		} else if (i != end(_tdbOrderInFilterMap)) {
			i->second = order;
		} else {
			_tdbOrderInFilterMap.emplace(filterId, order);
		}
	} else {
		if (_tdbOrderInChatList == order && pinnedUnchanged()) {
			return;
		}
		_tdbOrderInChatList = order;
	}
	if (const auto history = asHistory()) {
		// Pinned index depends on Tdb order.
		owner().setChatPinned(history, filterId, pinned);
	}
	refreshChatListSortPositionFromTdb(filterId, order);
}

void Entry::refreshChatListSortPositionFromTdb(
		FilterId filterId,
		uint64 order) {
	const auto fixedIndex = filterId ? 0 : fixedOnTopIndex();
	const auto index = order ? lookupPinnedIndex(filterId) : 0;
	const auto sortKey = fixedIndex
		? FixedOnTopDialogPos(fixedIndex)
		: index
		? PinnedDialogPos(index)
		: order;
	if (filterId) {
		if (order) {
			_sortKeyInFilterMap[filterId] = sortKey;
		} else {
			_sortKeyInFilterMap.remove(filterId);
		}
	} else {
		_sortKeyInChatList = sortKey;
	}
	if (sortKey) {
		owner().refreshChatListEntry(this, filterId);
		updateChatListEntry();
	} else {
		owner().removeChatListEntry(this, filterId);
	}
}

uint64 Entry::tdbOrderInChatList(FilterId filterId) const {
	if (filterId) {
		const auto i = _tdbOrderInFilterMap.find(filterId);
		return (i != end(_tdbOrderInFilterMap)) ? i->second : uint64(0);
	}
	return _tdbOrderInChatList;
}

int Entry::lookupPinnedIndex(FilterId filterId) const {
	if (filterId) {
		const auto i = _pinnedIndex.find(filterId);
		return (i != end(_pinnedIndex)) ? i->second : 0;
	} else if (!_pinnedIndex.empty()) {
		return _pinnedIndex.front().first
			? 0
			: _pinnedIndex.front().second;
	}
	return 0;
}

uint64 Entry::computeSortPosition(FilterId filterId) const {
	Expects(filterId != 0);
#if 0 // mtp
	const auto index = lookupPinnedIndex(filterId);
	return index ? PinnedDialogPos(index) : _sortKeyByDate;
#endif
	const auto i = _sortKeyInFilterMap.find(filterId);
	return (i != end(_sortKeyInFilterMap)) ? i->second : 0ULL;
}

void Entry::updateChatListExistence() {
#if 0 // mtp
	setChatListExistence(shouldBeInChatList());
#endif
	if (const auto folder = asFolder()) {
		const auto order = folder->chatsList()->empty()
			? 0
			: FixedOnTopDialogPos(kArchiveFixOnTopIndex);
		updateChatListSortPosition(FilterId(), order, false);
	}
}

void Entry::notifyUnreadStateChange(const UnreadState &wasState) {
	Expects(folderKnown());
	Expects(inChatList());

	const auto nowState = chatListUnreadState();
	owner().chatsListFor(this)->unreadStateChanged(wasState, nowState);
	auto &filters = owner().chatsFilters();
	for (const auto &[filterId, links] : _chatListLinks) {
		if (filterId) {
			filters.chatsList(filterId)->unreadStateChanged(
				wasState,
				nowState);
		}
	}
	if (const auto history = asHistory()) {
		session().changes().historyUpdated(
			history,
			Data::HistoryUpdate::Flag::UnreadView);
		const auto isForFilters = [](UnreadState state) {
			return state.messages || state.marks || state.mentions;
		};
		if (isForFilters(wasState) != isForFilters(nowState)) {
			owner().chatsFilters().refreshHistory(history);
		}
	}
	updateChatListEntryPostponed();
}

const Ui::Text::String &Entry::chatListNameText() const {
	const auto version = chatListNameVersion();
	if (_chatListNameVersion < version) {
		_chatListNameVersion = version;
		_chatListNameText.setText(
			st::semiboldTextStyle,
			chatListName(),
			Ui::NameTextOptions());
	}
	return _chatListNameText;
}

#if 0 // mtp
void Entry::setChatListExistence(bool exists) {
	if (exists && _sortKeyInChatList) {
		owner().refreshChatListEntry(this);
		updateChatListEntry();
	} else {
		owner().removeChatListEntry(this);
	}
}

TimeId Entry::adjustedChatListTimeId() const {
	return chatListTimeId();
}
#endif

void Entry::changedChatListPinHook() {
}

RowsByLetter *Entry::chatListLinks(FilterId filterId) {
	const auto i = _chatListLinks.find(filterId);
	return (i != end(_chatListLinks)) ? &i->second : nullptr;
}

const RowsByLetter *Entry::chatListLinks(FilterId filterId) const {
	const auto i = _chatListLinks.find(filterId);
	return (i != end(_chatListLinks)) ? &i->second : nullptr;
}

not_null<Row*> Entry::mainChatListLink(FilterId filterId) const {
	const auto links = chatListLinks(filterId);
	Assert(links != nullptr);
	return links->main;
}

Row *Entry::maybeMainChatListLink(FilterId filterId) const {
	const auto links = chatListLinks(filterId);
	return links ? links->main.get() : nullptr;
}

PositionChange Entry::adjustByPosInChatList(
		FilterId filterId,
		not_null<MainList*> list) {
	const auto links = chatListLinks(filterId);
	Assert(links != nullptr);
	const auto from = links->main->top();
	list->indexed()->adjustByDate(*links);
	const auto to = links->main->top();
	return { .from = from, .to = to, .height = links->main->height() };
}

void Entry::setChatListTimeId(TimeId date) {
#if 0 // mtp
	_timeId = date;
	updateChatListSortPosition();
	if (const auto folder = this->folder()) {
		folder->updateChatListSortPosition();
	}
#endif
	if (const auto topic = asTopic()) {
		// tdlib won't be needed when tdlib manages topics
		_timeId = date;
		updateChatListSortPosition();
	}
}

int Entry::posInChatList(FilterId filterId) const {
	return mainChatListLink(filterId)->index();
}

not_null<Row*> Entry::addToChatList(
		FilterId filterId,
		not_null<MainList*> list) {
	if (const auto main = maybeMainChatListLink(filterId)) {
		return main;
	}
	return _chatListLinks.emplace(
		filterId,
		list->addEntry(this)
	).first->second.main;
}

void Entry::removeFromChatList(
		FilterId filterId,
		not_null<MainList*> list) {
	if (isPinnedDialog(filterId)) {
		owner().setChatPinned(this, filterId, false);
	}

	const auto i = _chatListLinks.find(filterId);
	if (i == end(_chatListLinks)) {
		return;
	}
	_chatListLinks.erase(i);
	list->removeEntry(this);
}

void Entry::removeChatListEntryByLetter(FilterId filterId, QChar letter) {
	const auto i = _chatListLinks.find(filterId);
	if (i != end(_chatListLinks)) {
		i->second.letters.remove(letter);
	}
}

void Entry::addChatListEntryByLetter(
		FilterId filterId,
		QChar letter,
		not_null<Row*> row) {
	const auto i = _chatListLinks.find(filterId);
	if (i != end(_chatListLinks)) {
		i->second.letters.emplace(letter, row);
	}
}

void Entry::updateChatListEntry() {
	_flags &= ~Flag::UpdatePostponed;
	session().changes().entryUpdated(this, Data::EntryUpdate::Flag::Repaint);
}

void Entry::updateChatListEntryPostponed() {
	if (_flags & Flag::UpdatePostponed) {
		return;
	}
	_flags |= Flag::UpdatePostponed;
	Ui::PostponeCall(this, [=] {
		if (_flags & Flag::UpdatePostponed) {
			updateChatListEntry();
		}
	});
}

void Entry::updateChatListEntryHeight() {
	session().changes().entryUpdated(this, Data::EntryUpdate::Flag::Height);
}

} // namespace Dialogs
