/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/text/text_entity.h"
#include "tdb/tdb_tl_scheme.h"

namespace Main {
class Session;
} // namespace Main

namespace Api {

enum class ConvertOption {
	WithLocal,
	SkipLocal,
};

[[nodiscard]] EntitiesInText EntitiesFromMTP(
	Main::Session *session,
	const QVector<MTPMessageEntity> &entities);

[[nodiscard]] MTPVector<MTPMessageEntity> EntitiesToMTP(
	not_null<Main::Session*> session,
	const EntitiesInText &entities,
	ConvertOption option = ConvertOption::WithLocal);

[[nodiscard]] EntitiesInText EntitiesFromTdb(
	const QVector<Tdb::TLtextEntity> &entities);

[[nodiscard]] TextWithEntities FormattedTextFromTdb(
	const Tdb::TLformattedText &text);

[[nodiscard]] Tdb::TLvector<Tdb::TLtextEntity> EntitiesToTdb(
	const EntitiesInText &entities);

[[nodiscard]] Tdb::TLformattedText FormattedTextToTdb(
	const TextWithEntities &text);

} // namespace Api
