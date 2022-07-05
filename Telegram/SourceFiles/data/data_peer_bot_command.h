/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Tdb {
class TLbotCommand;
} // namespace Tdb

namespace Data {

struct BotCommand final {
	QString command;
	QString description;

	inline bool operator==(const BotCommand &other) const {
		return (command == other.command)
			&& (description == other.description);
	}
	inline bool operator!=(const BotCommand &other) const {
		return !(*this == other);
	}
};

#if 0 // mtp
[[nodiscard]] BotCommand BotCommandFromTL(const MTPBotCommand &result);
#endif
[[nodiscard]] BotCommand BotCommandFromTL(const Tdb::TLbotCommand &result);

} // namespace Data
