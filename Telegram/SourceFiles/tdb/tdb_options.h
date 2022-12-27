/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Tdb {

class Sender;

class TLDupdateOption;

class Options final {
public:
	explicit Options(not_null<Sender*> sender);

	bool consume(const TLDupdateOption &update);

	[[nodiscard]] int antiSpamGroupSizeMin() const {
		return _antiSpamGroupSizeMin;
	}
	[[nodiscard]] int64 antiSpamBotUserId() const {
		return _antiSpamBotUserId;
	}

private:
	int _antiSpamGroupSizeMin = 100;
	int64 _antiSpamBotUserId = 0;

};

} // namespace Tdb
