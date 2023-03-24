/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb_tl-scheme.h"
#include "tdb/tdb_error.h"

namespace Tdb {

// 1 - autoupdate download through TDLib.
inline constexpr auto kDefaultDownloadPriority = 2;

} // namespace Tdb
