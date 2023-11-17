/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_statistics_data_deserialize.h"

#include "data/data_statistics_chart.h"
#include "statistics/statistics_data_deserialize.h"

#include "tdb/tdb_tl_scheme.h"

namespace Api {

#if 0 // mtp
Data::StatisticalGraph StatisticalGraphFromTL(const MTPStatsGraph &tl) {
	return tl.match([&](const MTPDstatsGraph &d) {
		using namespace Statistic;
		const auto zoomToken = d.vzoom_token().has_value()
			? qs(*d.vzoom_token()).toUtf8()
			: QByteArray();
		return Data::StatisticalGraph{
			StatisticalChartFromJSON(qs(d.vjson().data().vdata()).toUtf8()),
			zoomToken,
		};
	}, [&](const MTPDstatsGraphAsync &data) {
		return Data::StatisticalGraph{
			.zoomToken = qs(data.vtoken()).toUtf8(),
		};
	}, [&](const MTPDstatsGraphError &data) {
		return Data::StatisticalGraph{ .error = qs(data.verror()) };
	});
}
#endif

Data::StatisticalGraph StatisticalGraphFromTL(
		const Tdb::TLstatisticalGraph &tl) {
	return tl.match([&](const Tdb::TLDstatisticalGraphData &data) {
		using namespace Statistic;
		return Data::StatisticalGraph{
			StatisticalChartFromJSON(data.vjson_data().v.toUtf8()),
			data.vzoom_token().v.toUtf8(),
		};
	}, [&](const Tdb::TLDstatisticalGraphAsync &data) {
		return Data::StatisticalGraph{
			.zoomToken = data.vtoken().v.toUtf8(),
		};
	}, [&](const Tdb::TLDstatisticalGraphError &data) {
		return Data::StatisticalGraph{
			.error = data.verror_message().v.toUtf8(),
		};
	});
}

} // namespace Api
