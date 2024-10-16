/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_peer_bot_commands.h"

#include "tdb/tdb_tl_scheme.h"

namespace Data {

ChatBotCommands::Changed ChatBotCommands::update(
		const std::vector<BotCommands> &list) {
	auto changed = false;
	if (list.empty()) {
		changed = (list.empty() != empty());
		clear();
	} else {
		for (const auto &commands : list) {
			auto &value = operator[](commands.userId);
			changed |= commands.commands.empty()
				? remove(commands.userId)
				: !ranges::equal(value, commands.commands);
			value = commands.commands;
		}
	}
	return changed;
}

#if 0 // mtp
BotCommands BotCommandsFromTL(const MTPBotInfo &result) {
	return result.match([](const MTPDbotInfo &data) {
		const auto userId = data.vuser_id()
			? UserId(*data.vuser_id())
			: UserId();
		if (!data.vcommands()) {
			return BotCommands{ .userId = userId };
		}
		auto commands = ranges::views::all(
			data.vcommands()->v
		) | ranges::views::transform(BotCommandFromTL) | ranges::to_vector;
		return BotCommands{
			.userId = userId,
			.commands = std::move(commands),
		};
	});
}
#endif
BotCommands BotCommandsFromTL(const Tdb::TLbotCommands &result) {
	auto commands = ranges::views::all(
		result.data().vcommands().v
	) | ranges::views::transform(BotCommandFromTL) | ranges::to_vector;
	return BotCommands{
		.userId = UserId(result.data().vbot_user_id().v),
		.commands = std::move(commands),
	};
}

} // namespace Data
