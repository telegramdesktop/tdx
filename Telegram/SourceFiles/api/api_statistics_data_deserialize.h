/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Tdb {
class TLstatisticalGraph;
} // namespace Tdb

namespace Data {
struct StatisticalGraph;
} // namespace Data

namespace Api {

#if 0 // mtp
[[nodiscard]] Data::StatisticalGraph StatisticalGraphFromTL(
	const MTPStatsGraph &tl);
#endif
[[nodiscard]] Data::StatisticalGraph StatisticalGraphFromTL(
	const Tdb::TLstatisticalGraph &tl);

} // namespace Api
