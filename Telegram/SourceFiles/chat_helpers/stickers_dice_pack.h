/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Tdb {
class TLDupdateDiceEmojis;
class TLDmessageDice;
} // namespace Tdb

class DocumentData;

namespace Main {
class Session;
} // namespace Main

namespace Stickers {

class DicePack final {
public:
	DicePack(not_null<Main::Session*> session, const QString &emoji);
	~DicePack();

	[[nodiscard]] DocumentData *lookup(int value);

	void apply(const Tdb::TLDmessageDice &data);

private:
	void load();
	void applySet(const MTPDmessages_stickerSet &data);
	void tryGenerateLocalZero();

	const not_null<Main::Session*> _session;
	QString _emoji;
	base::flat_map<int, not_null<DocumentData*>> _map;
	mtpRequestId _requestId = 0;

};

class DicePacks final {
public:
	explicit DicePacks(not_null<Main::Session*> session);

	static const QString kDiceString;
	static const QString kDartString;
	static const QString kSlotString;
	static const QString kFballString;
	static const QString kBballString;
	static const QString kPartyPopper;

	[[nodiscard]] static bool IsSlot(const QString &emoji) {
		return (emoji == kSlotString);
	}

	[[nodiscard]] DocumentData *lookup(const QString &emoji, int value);

	void apply(const Tdb::TLDupdateDiceEmojis &update);
	[[nodiscard]] const std::vector<QString> &cloudDiceEmoticons() const;

	void apply(const Tdb::TLDmessageDice &data);

private:
	const not_null<Main::Session*> _session;

	base::flat_map<QString, std::unique_ptr<DicePack>> _packs;

	std::vector<QString> _cloudDiceEmoticons;

};

} // namespace Stickers
