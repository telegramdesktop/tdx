/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class History;

namespace Data {
class SavedMessages;
class Session;
class Forum;
} // namespace Data

namespace Dialogs {

class Key;

class PinnedList final {
public:
	PinnedList(FilterId filterId, int limit);

	void setLimit(int limit);

#if 0 // mtp
	// Places on the last place in the list otherwise.
	// Does nothing if already pinned.
	void addPinned(Key key);
#endif

	// if (pinned) places on the first place in the list.
	void setPinned(Key key, bool pinned);

	void clear();

#if 0 // mtp
	void applyList(
		not_null<Data::Session*> owner,
		const QVector<MTPDialogPeer> &list);
	void applyList(
		not_null<Data::SavedMessages*> sublistsOwner,
		const QVector<MTPDialogPeer> &list);
	void applyList(
		not_null<Data::Forum*> forum,
		const QVector<MTPint> &list);
#endif

	void applyList(const std::vector<not_null<History*>> &list);
	void reorder(Key key1, Key key2);

	const std::vector<Key> &order() const {
		return _data;
	}

private:
#if 0 // mtp
	int addPinnedGetPosition(Key key);
#endif
	void applyLimit(int limit);

	FilterId _filterId = 0;
	int _limit = 0;
	std::vector<Key> _data;

};

} // namespace Dialogs
