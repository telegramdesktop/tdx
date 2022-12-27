/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/tdb_options.h"

#include "tdb/tdb_option.h"
#include "base/unixtime.h"

namespace Tdb {

Options::Options(not_null<Sender*> sender) {
}

bool Options::consume(const TLDupdateOption &update) {
	const auto &name = update.vname().v;
	const auto &value = update.vvalue();
	if (name == u"aggressive_anti_spam_supergroup_member_count_min"_q) {
		_antiSpamGroupSizeMin = OptionValue<int64>(value);
	} else if (name == u"anti_spam_bot_user_id"_q) {
		_antiSpamBotUserId = OptionValue<int64>(value);
	} else if (name == u"unix_time"_q) {
		base::unixtime::update(OptionValue<int64>(value));
	} else if (name == u"channel_custom_accent_color_boost_level_min"_q) {
		_channelCustomAccentColorBoostLevelMin = OptionValue<int64>(value);
	} else {
		return false;
	}
	return true;
}

} // namespace Tdb
