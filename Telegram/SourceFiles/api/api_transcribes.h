/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/tdb_sender.h"
#if 0 // mtp
#include "mtproto/sender.h"
#endif

namespace Tdb {
class TLspeechRecognitionResult;
} // namespace Tdb

class ApiWrap;

namespace Main {
class Session;
} // namespace Main

namespace Api {

class Transcribes final {
public:
	explicit Transcribes(not_null<ApiWrap*> api);

	struct Entry {
		QString result;
		bool shown = false;
		bool failed = false;
		bool toolong = false;
		bool pending = false;
		bool roundview = false;
		mtpRequestId requestId = 0;
	};

	void toggle(not_null<HistoryItem*> item);
	[[nodiscard]] const Entry &entry(not_null<HistoryItem*> item) const;

	void apply(
		not_null<HistoryItem*> item,
		const Tdb::TLspeechRecognitionResult &result,
		bool roundview);
#if 0 // mtp
	void apply(const MTPDupdateTranscribedAudio &update);
#endif

private:
	void load(not_null<HistoryItem*> item);

	const not_null<Main::Session*> _session;
#if 0 // mtp
	MTP::Sender _api;
#endif
	Tdb::Sender _api;

	base::flat_map<FullMsgId, Entry> _map;
	base::flat_map<uint64, FullMsgId> _ids;

};

} // namespace Api
