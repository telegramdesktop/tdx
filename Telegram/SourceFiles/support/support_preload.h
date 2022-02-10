/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class History;

namespace Support {

// Returns histories().request, not api().request.
#if 0 // mtp
[[nodiscard]] int SendPreloadRequest(
#endif
[[nodiscard]] uint64 SendPreloadRequest(
	not_null<History*> history,
	Fn<void()> retry);

} // namespace Support
