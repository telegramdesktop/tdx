/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_bot_app.h"

#include "tdb/tdb_tl_scheme.h"
#include "data/data_session.h"

BotAppData::BotAppData(not_null<Data::Session*> owner, const BotAppId &id)
: owner(owner)
, id(id) {
}

void BotAppData::apply(PeerId bot, const Tdb::TLwebApp &app) {
	const auto &data = app.data();
	botId = bot;
	shortName = data.vshort_name().v;
	title = data.vtitle().v;
	description = data.vdescription().v;
	photo = owner->processPhoto(data.vphoto());
	if (const auto animation = data.vanimation()) {
		document = owner->processDocument(*animation);
	}
}
