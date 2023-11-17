/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "mtproto/sender.h"

class ChannelData;
class PeerData;
class UserData;

namespace Api {

class StatisticsRequestSender {
protected:
	explicit StatisticsRequestSender(not_null<PeerData*> peer);
#if 0 // mtp
	~StatisticsRequestSender();

	template <
		typename Request,
		typename = std::enable_if_t<!std::is_reference_v<Request>>,
		typename = typename Request::Unboxed>
	[[nodiscard]] auto makeRequest(Request &&request) {
		const auto [id, dcId] = ensureRequestIsRegistered();
		return std::move(_api.request(
			std::forward<Request>(request)
		).toDC(
			dcId ? MTP::ShiftDcId(dcId, MTP::kStatsDcShift) : 0
		).overrideId(id));
	}
#endif

	[[nodiscard]] MTP::Sender &api();
	[[nodiscard]] not_null<ChannelData*> channel();
	[[nodiscard]] not_null<UserData*> user();

private:
#if 0 // mtp
	struct Registered final {
		mtpRequestId id;
		MTP::DcId dcId;
	};
	[[nodiscard]] Registered ensureRequestIsRegistered();
	void checkRequests();
#endif

	const not_null<PeerData*> _peer;
	ChannelData * const _channel;
	UserData * const _user;
	MTP::Sender _api;
#if 0 // mtp
	base::Timer _timer;
	base::flat_map<MTP::DcId, base::flat_set<mtpRequestId>> _requests;
#endif

};

} // namespace Api
