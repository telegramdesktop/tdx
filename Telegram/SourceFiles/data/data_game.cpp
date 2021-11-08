/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_game.h"

#include "data/data_session.h"
#include "tdb/tdb_tl_scheme.h"

GameData::GameData(not_null<Data::Session*> owner, const GameId &id)
: owner(owner)
, id(id) {
}

GameId GameData::IdFromTdb(const Tdb::TLgame &data) {
	return data.data().vid().v;
}

void GameData::setFromTdb(const Tdb::TLgame &data) {
	const auto &fields = data.data();
	if (const auto animation = fields.vanimation()) {
		document = owner->processDocument(*animation);
	}
	photo = owner->processPhoto(fields.vphoto());
	description = fields.vdescription().v;
	title = fields.vtitle().v;
	shortName = fields.vshort_name().v;
	//fields.vtext(); // todo
}
