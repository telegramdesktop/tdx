/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/filter_icons.h"

#include "ui/emoji_config.h"
#include "data/data_chat_filters.h"
#include "styles/style_filter_icons.h"

namespace Ui {
namespace {

#if 0 // mtp
const auto kIcons = std::vector<FilterIcons>{
	{
		&st::foldersCat,
		&st::foldersCatActive,
		"\xF0\x9F\x90\xB1"_cs.utf16()
	},
	{
		&st::foldersBook,
		&st::foldersBookActive,
		"\xF0\x9F\x93\x95"_cs.utf16()
	},
	{
		&st::foldersMoney,
		&st::foldersMoneyActive,
		"\xF0\x9F\x92\xB0"_cs.utf16()
	},
	//{
	//	&st::foldersCamera,
	//	&st::foldersCameraActive,
	//	"\xF0\x9F\x93\xB8"_cs.utf16()
	//},
	{
		&st::foldersGame,
		&st::foldersGameActive,
		"\xF0\x9F\x8E\xAE"_cs.utf16()
	},
	//{
	//	&st::foldersHouse,
	//	&st::foldersHouseActive,
	//	"\xF0\x9F\x8F\xA1"_cs.utf16()
	//},
	{
		&st::foldersLight,
		&st::foldersLightActive,
		"\xF0\x9F\x92\xA1"_cs.utf16()
	},
	{
		&st::foldersLike,
		&st::foldersLikeActive,
		"\xF0\x9F\x91\x8C"_cs.utf16()
	},
	//{
	//	&st::foldersPlus,
	//	&st::foldersPlusActive,
	//	"\xE2\x9E\x95"_cs.utf16()
	//},
	{
		&st::foldersNote,
		&st::foldersNoteActive,
		"\xF0\x9F\x8E\xB5"_cs.utf16()
	},
	{
		&st::foldersPalette,
		&st::foldersPaletteActive,
		"\xF0\x9F\x8E\xA8"_cs.utf16()
	},
	{
		&st::foldersTravel,
		&st::foldersTravelActive,
		"\xE2\x9C\x88\xEF\xB8\x8F"_cs.utf16()
	},
	{
		&st::foldersSport,
		&st::foldersSportActive,
		"\xE2\x9A\xBD\xEF\xB8\x8F"_cs.utf16()
	},
	{
		&st::foldersFavorite,
		&st::foldersFavoriteActive,
		"\xE2\xAD\x90"_cs.utf16()
	},
	{
		&st::foldersStudy,
		&st::foldersStudyActive,
		"\xF0\x9F\x8E\x93"_cs.utf16()
	},
	{
		&st::foldersAirplane,
		&st::foldersAirplaneActive,
		"\xF0\x9F\x9B\xAB"_cs.utf16()
	},
	//{
	//	&st::foldersMicrobe,
	//	&st::foldersMicrobeActive,
	//	"\xF0\x9F\xA6\xA0"_cs.utf16()
	//},
	//{
	//	&st::foldersWorker,
	//	&st::foldersWorkerActive,
	//	"\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x92\xBC"_cs.utf16()
	//},
	{
		&st::foldersPrivate,
		&st::foldersPrivateActive,
		"\xF0\x9F\x91\xA4"_cs.utf16()
	},
	{
		&st::foldersGroups,
		&st::foldersGroupsActive,
		"\xF0\x9F\x91\xA5"_cs.utf16()
	},
	{
		&st::foldersAll,
		&st::foldersAllActive,
		"\xF0\x9F\x92\xAC"_cs.utf16()
	},
	{
		&st::foldersUnread,
		&st::foldersUnreadActive,
		"\xE2\x9C\x85"_cs.utf16()
	},
	//{
	//	&st::foldersCheck,
	//	&st::foldersCheckActive,
	//	"\xE2\x98\x91\xEF\xB8\x8F"_cs.utf16()
	//},
	{
		&st::foldersBots,
		&st::foldersBotsActive,
		"\xF0\x9F\xA4\x96"_cs.utf16()
	},
	//{
	//	&st::foldersFolders,
	//	&st::foldersFoldersActive,
	//	"\xF0\x9F\x97\x82"_cs.utf16()
	//},
	{
		&st::foldersCrown,
		&st::foldersCrownActive,
		"\xF0\x9F\x91\x91"_cs.utf16()
	},
	{
		&st::foldersFlower,
		&st::foldersFlowerActive,
		"\xF0\x9F\x8C\xB9"_cs.utf16()
	},
	{
		&st::foldersHome,
		&st::foldersHomeActive,
		"\xF0\x9F\x8F\xA0"_cs.utf16()
	},
	{
		&st::foldersLove,
		&st::foldersLoveActive,
		"\xE2\x9D\xA4"_cs.utf16()
	},
	{
		&st::foldersMask,
		&st::foldersMaskActive,
		"\xF0\x9F\x8E\xAD"_cs.utf16()
	},
	{
		&st::foldersParty,
		&st::foldersPartyActive,
		"\xF0\x9F\x8D\xB8"_cs.utf16()
	},
	{
		&st::foldersTrade,
		&st::foldersTradeActive,
		"\xF0\x9F\x93\x88"_cs.utf16()
	},
	{
		&st::foldersWork,
		&st::foldersWorkActive,
		"\xF0\x9F\x92\xBC"_cs.utf16()
	},
	{
		&st::foldersUnmuted,
		&st::foldersUnmutedActive,
		"\xF0\x9F\x94\x94"_cs.utf16()
	},
	{
		&st::foldersChannels,
		&st::foldersChannelsActive,
		"\xF0\x9F\x93\xA2"_cs.utf16()
	},
	{
		&st::foldersCustom,
		&st::foldersCustomActive,
		"\xF0\x9F\x93\x81"_cs.utf16()
	},
	{
		&st::foldersSetup,
		&st::foldersSetupActive,
		"\xF0\x9F\x93\x8B"_cs.utf16()
	},
	//{
	//	&st::foldersPoo,
	//	&st::foldersPooActive,
	//	"\xF0\x9F\x92\xA9"_cs.utf16()
	//},
	{
		&st::filtersEdit,
		&st::filtersEdit,
		QString()
	}
};
#endif

const auto kIcons = std::vector<FilterIcons>{
	{
		&st::foldersCat,
		&st::foldersCatActive,
		u"Cat"_q,
	},
	{
		&st::foldersBook,
		&st::foldersBookActive,
		u"Book"_q,
	},
	{
		&st::foldersMoney,
		&st::foldersMoneyActive,
		u"Money"_q,
	},
	//{
	//	&st::foldersCamera,
	//	&st::foldersCameraActive,
	//	u"Camera"_q,
	//},
	{
		&st::foldersGame,
		&st::foldersGameActive,
		u"Game"_q,
	},
	//{
	//	&st::foldersHouse,
	//	&st::foldersHouseActive,
	//	u"House"_q,
	//},
	{
		&st::foldersLight,
		&st::foldersLightActive,
		u"Light"_q,
	},
	{
		&st::foldersLike,
		&st::foldersLikeActive,
		u"Like"_q,
	},
	//{
	//	&st::foldersPlus,
	//	&st::foldersPlusActive,
	//	u"Plus"_q
	//},
	{
		&st::foldersNote,
		&st::foldersNoteActive,
		u"Note"_q,
	},
	{
		&st::foldersPalette,
		&st::foldersPaletteActive,
		u"Palette"_q,
	},
	{
		&st::foldersTravel,
		&st::foldersTravelActive,
		u"Travel"_q,
	},
	{
		&st::foldersSport,
		&st::foldersSportActive,
		u"Sport"_q,
	},
	{
		&st::foldersFavorite,
		&st::foldersFavoriteActive,
		u"Favorite"_q,
	},
	{
		&st::foldersStudy,
		&st::foldersStudyActive,
		u"Study"_q,
	},
	{
		&st::foldersAirplane,
		&st::foldersAirplaneActive,
		u"Airplane"_q,
	},
	//{
	//	&st::foldersMicrobe,
	//	&st::foldersMicrobeActive,
	//	u"Microbe"_q,
	//},
	//{
	//	&st::foldersWorker,
	//	&st::foldersWorkerActive,
	//	u"Worker"_q,
	//},
	{
		&st::foldersPrivate,
		&st::foldersPrivateActive,
		u"Private"_q,
	},
	{
		&st::foldersGroups,
		&st::foldersGroupsActive,
		u"Groups"_q,
	},
	{
		&st::foldersAll,
		&st::foldersAllActive,
		u"All"_q,
	},
	{
		&st::foldersUnread,
		&st::foldersUnreadActive,
		u"Unread"_q,
	},
	//{
	//	&st::foldersCheck,
	//	&st::foldersCheckActive,
	//	u"Check"_q,
	//},
	{
		&st::foldersBots,
		&st::foldersBotsActive,
		u"Bots"_q,
	},
	//{
	//	&st::foldersFolders,
	//	&st::foldersFoldersActive,
	//	u"Folders"_q,
	//},
	{
		&st::foldersCrown,
		&st::foldersCrownActive,
		u"Crown"_q,
	},
	{
		&st::foldersFlower,
		&st::foldersFlowerActive,
		u"Flower"_q,
	},
	{
		&st::foldersHome,
		&st::foldersHomeActive,
		u"Home"_q,
	},
	{
		&st::foldersLove,
		&st::foldersLoveActive,
		u"Love"_q,
	},
	{
		&st::foldersMask,
		&st::foldersMaskActive,
		u"Mask"_q,
	},
	{
		&st::foldersParty,
		&st::foldersPartyActive,
		u"Party"_q,
	},
	{
		&st::foldersTrade,
		&st::foldersTradeActive,
		u"Trade"_q,
	},
	{
		&st::foldersWork,
		&st::foldersWorkActive,
		u"Work"_q,
	},
	{
		&st::foldersUnmuted,
		&st::foldersUnmutedActive,
		u"Unmuted"_q,
	},
	{
		&st::foldersChannels,
		&st::foldersChannelsActive,
		u"Channels"_q,
	},
	{
		&st::foldersCustom,
		&st::foldersCustomActive,
		u"Custom"_q,
	},
	{
		&st::foldersSetup,
		&st::foldersSetupActive,
		u"Setup"_q,
	},
	//{
	//	&st::foldersPoo,
	//	&st::foldersPooActive,
	//	u"Poo"_q,
	//},
	{
		&st::filtersEdit,
		&st::filtersEdit,
		QString()
	}
};

} // namespace

const FilterIcons &LookupFilterIcon(FilterIcon icon) {
	Expects(static_cast<int>(icon) < kIcons.size());

	return kIcons[static_cast<int>(icon)];
}

std::optional<FilterIcon> LookupFilterIconByEmoji(const QString &emoji) {
#if 0 // mtp
	static const auto kMap = [] {
		auto result = base::flat_map<EmojiPtr, FilterIcon>();
		auto index = 0;
		for (const auto &entry : kIcons) {
			if (entry.emoji.isEmpty()) {
				continue;
			}
			const auto emoji = Ui::Emoji::Find(entry.emoji);
			Assert(emoji != nullptr);
			result.emplace(emoji, static_cast<FilterIcon>(index++));
		}
		return result;
	}();
	const auto i = kMap.find(Ui::Emoji::Find(emoji));
#endif
	static const auto kMap = [] {
		auto result = base::flat_map<QString, FilterIcon>();
		auto index = 0;
		for (const auto &entry : kIcons) {
			if (entry.emoji.isEmpty()) {
				continue;
			}
			result.emplace(entry.emoji, static_cast<FilterIcon>(index++));
		}
		return result;
	}();
	const auto i = kMap.find(emoji);
	return (i != end(kMap)) ? std::make_optional(i->second) : std::nullopt;
}

FilterIcon ComputeDefaultFilterIcon(const Data::ChatFilter &filter) {
	using Icon = FilterIcon;
	using Flag = Data::ChatFilter::Flag;

	const auto all = Flag::Contacts
		| Flag::NonContacts
		| Flag::Groups
		| Flag::Channels
		| Flag::Bots;
	const auto removed = Flag::NoRead | Flag::NoMuted;
	const auto people = Flag::Contacts | Flag::NonContacts;
	if (!filter.always().empty()
		|| !filter.never().empty()
		|| !(filter.flags() & all)) {
		return Icon::Custom;
	} else if ((filter.flags() & all) == Flag::Contacts
		|| (filter.flags() & all) == Flag::NonContacts
		|| (filter.flags() & all) == people) {
		return Icon::Private;
	} else if ((filter.flags() & all) == Flag::Groups) {
		return Icon::Groups;
	} else if ((filter.flags() & all) == Flag::Channels) {
		return Icon::Channels;
	} else if ((filter.flags() & all) == Flag::Bots) {
		return Icon::Bots;
	} else if ((filter.flags() & removed) == Flag::NoRead) {
		return Icon::Unread;
	} else if ((filter.flags() & removed) == Flag::NoMuted) {
		return Icon::Unmuted;
	}
	return Icon::Custom;
}

FilterIcon ComputeFilterIcon(const Data::ChatFilter &filter) {
	return LookupFilterIconByEmoji(filter.iconEmoji()).value_or(
		ComputeDefaultFilterIcon(filter));
}

} // namespace Ui
