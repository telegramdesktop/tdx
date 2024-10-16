/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/tdb_tl_scheme.h"

namespace Iv {

struct Options;
struct Prepared;

struct Source {
	uint64 pageId = 0;
#if 0 // mtp
	MTPPage page;
	std::optional<MTPPhoto> webpagePhoto;
	std::optional<MTPDocument> webpageDocument;
#endif
	Tdb::TLwebPageInstantView page;
	QString url;
	QString name;
	int updatedCachedViews = 0;
};

[[nodiscard]] Prepared Prepare(const Source &source, const Options &options);

} // namespace Iv
