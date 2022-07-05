/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_peer_bot_command.h"

#include "tdb/tdb_tl_scheme.h"

namespace Data {

#if 0 // mtp
BotCommand BotCommandFromTL(const MTPBotCommand &result) {
	return result.match([](const MTPDbotCommand &data) {
		return BotCommand {
			.command = qs(data.vcommand().v),
			.description = qs(data.vdescription().v),
		};
	});
}
#endif

BotCommand BotCommandFromTL(const Tdb::TLbotCommand &result) {
	const auto &data = result.data();
	return BotCommand{
		.command = data.vcommand().v,
		.description = data.vdescription().v,
	};
}

} // namespace Data
