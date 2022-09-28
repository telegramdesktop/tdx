/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#if 0 // mtp
#include "mtproto/sender.h"
#endif
#include "base/timer.h"

class ApiWrap;
class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace Api {

class ViewsManager final {
public:
	explicit ViewsManager(not_null<ApiWrap*> api);

	void scheduleIncrement(not_null<HistoryItem*> item);
	void removeIncremented(not_null<PeerData*> peer);

#if 0 // mtp
	void pollExtendedMedia(not_null<HistoryItem*> item, bool force = false);
#endif

private:
	struct PollExtendedMediaRequest {
		crl::time when = 0;
		mtpRequestId id = 0;
		base::flat_set<MsgId> ids;
		base::flat_set<MsgId> sent;
		bool forced = false;
	};

	void viewsIncrement();

#if 0 // mtp
	void sendPollRequests();
	void sendPollRequests(
		const base::flat_map<
			not_null<PeerData*>,
			QVector<MTPint>> &prepared);

	void done(
		QVector<MTPint> ids,
		const MTPmessages_MessageViews &result,
		mtpRequestId requestId);
	void fail(const MTP::Error &error, mtpRequestId requestId);
#endif

	const not_null<Main::Session*> _session;
#if 0 // mtp
	MTP::Sender _api;
#endif

	base::flat_map<not_null<PeerData*>, base::flat_set<MsgId>> _incremented;
	base::flat_map<not_null<PeerData*>, base::flat_set<MsgId>> _toIncrement;
	base::flat_map<not_null<PeerData*>, mtpRequestId> _incrementRequests;
	base::flat_map<mtpRequestId, not_null<PeerData*>> _incrementByRequest;
	base::Timer _incrementTimer;

#if 0 // mtp
	base::flat_map<
		not_null<PeerData*>,
		PollExtendedMediaRequest> _pollRequests;
	base::Timer _pollTimer;
#endif

};

} // namespace Api
