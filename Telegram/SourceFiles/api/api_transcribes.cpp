/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_transcribes.h"

#include "apiwrap.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "main/main_app_config.h"
#include "main/main_session.h"

#include "tdb/tdb_tl_scheme.h"

namespace Api {
namespace {

using namespace Tdb;

} // namespace

Transcribes::Transcribes(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance()) {
}

bool Transcribes::freeFor(not_null<HistoryItem*> item) const {
	if (const auto channel = item->history()->peer->asMegagroup()) {
		const auto owner = &channel->owner();
		return channel->levelHint() >= owner->groupFreeTranscribeLevel();
	}
	return false;
}

bool Transcribes::trialsSupport() {
#if 0 // mtp
	if (!_trialsSupport) {
		const auto count = _session->appConfig().get<int>(
			u"transcribe_audio_trial_weekly_number"_q,
			0);
		const auto until = _session->appConfig().get<int>(
			u"transcribe_audio_trial_cooldown_until"_q,
			0);
		_trialsSupport = (count > 0) || (until > 0);
	}
	return *_trialsSupport;
#endif
	return _trialsCount > 0 || _trialsRefreshAt > 0;
}

TimeId Transcribes::trialsRefreshAt() {
	if (_trialsRefreshAt < 0) {
#if 0 // mtp
		_trialsRefreshAt = _session->appConfig().get<int>(
			u"transcribe_audio_trial_cooldown_until"_q,
			0);
#endif
		return std::max(_trialsRefreshAt, 0);
	}
	return _trialsRefreshAt;
}

int Transcribes::trialsCount() {
	if (_trialsCount < 0) {
#if 0 // mtp
		_trialsCount = _session->appConfig().get<int>(
			u"transcribe_audio_trial_weekly_number"_q,
			-1);
#endif
		return std::max(_trialsCount, 0);
	}
	return _trialsCount;
}

crl::time Transcribes::trialsMaxLengthMs() const {
	return _trialsMaxLengthMs;
#if 0 // mtp
	return 1000 * _session->appConfig().get<int>(
		u"transcribe_audio_trial_duration_max"_q,
		300);
#endif
}

void Transcribes::toggle(not_null<HistoryItem*> item) {
	const auto id = item->fullId();
	auto i = _map.find(id);
	if (i == _map.end()) {
		load(item);
		//_session->data().requestItemRepaint(item);
		_session->data().requestItemResize(item);
	} else if (!i->second.requestId) {
		i->second.shown = !i->second.shown;
#if 0 // mtp
		if (i->second.roundview) {
#endif
		if (i->second.roundview && !i->second.pending) {
			_session->data().requestItemViewRefresh(item);
		}
		_session->data().requestItemResize(item);
	}
}

const Transcribes::Entry &Transcribes::entry(
		not_null<HistoryItem*> item) const {
	static auto empty = Entry();
	const auto i = _map.find(item->fullId());
	return (i != _map.end()) ? i->second : empty;
}

#if 0 // mtp
void Transcribes::apply(const MTPDupdateTranscribedAudio &update) {
	const auto id = update.vtranscription_id().v;
	const auto i = _ids.find(id);
	if (i == _ids.end()) {
		return;
	}
	const auto j = _map.find(i->second);
	if (j == _map.end()) {
		return;
	}
	const auto text = qs(update.vtext());
	j->second.result = text;
	j->second.pending = update.is_pending();
	if (const auto item = _session->data().message(i->second)) {
		if (j->second.roundview) {
			_session->data().requestItemViewRefresh(item);
		}
		_session->data().requestItemResize(item);
	}
}
#endif

void Transcribes::apply(
		not_null<HistoryItem*> item,
		const TLspeechRecognitionResult &result,
		bool roundview) {
	auto &entry = _map[item->fullId()];
	entry.roundview = roundview;
	result.match([&](const TLDspeechRecognitionResultText &result) {
		entry.requestId = 0;
		entry.result = result.vtext().v;
		entry.pending = false;
	}, [&](const TLDspeechRecognitionResultPending &result) {
		entry.result = result.vpartial_text().v;
		entry.pending = true;
	}, [&](const TLDspeechRecognitionResultError &result) {
		entry.requestId = 0;
		entry.pending = false;
		entry.failed = true;
		if (result.verror().data().vmessage().v == u"MSG_VOICE_TOO_LONG"_q) {
			entry.toolong = true;
		}
	});
	if (entry.roundview && !entry.pending) {
		_session->data().requestItemViewRefresh(item);
	}
	_session->data().requestItemResize(item);
}

void Transcribes::apply(const TLDupdateSpeechRecognitionTrial &data) {
	const auto trialsCount = data.vleft_count().v;
	const auto trialsRefreshAt = data.vnext_reset_date().v;
	const auto trialsCountChanged = (_trialsCount != trialsCount)
		&& (_trialsCount > 0); // Don't show toast on first update.
	const auto refreshAtChanged = (_trialsRefreshAt != trialsRefreshAt);
	_trialsMaxLengthMs = data.vmax_media_duration().v * crl::time(1000);
	_trialsCount = trialsCount;
	_trialsRefreshAt = trialsRefreshAt;
	if (trialsCountChanged) {
		ShowTrialTranscribesToast(_trialsCount, _trialsRefreshAt);
	}
}

void Transcribes::load(not_null<HistoryItem*> item) {
	if (!item->isHistoryEntry() || item->isLocal()) {
		return;
	}
	const auto toggleRound = [](not_null<HistoryItem*> item, Entry &entry) {
		if (const auto media = item->media()) {
			if (const auto document = media->document()) {
				if (document->isVideoMessage()) {
					entry.roundview = true;
					document->owner().requestItemViewRefresh(item);
				}
			}
		}
	};
	const auto id = item->fullId();
	const auto requestId = _api.request(TLrecognizeSpeech(
		peerToTdbChat(item->history()->peer->id),
		tl_int53(item->id.bare)
	)).done([=] {
		auto &entry = _map[id];
		if (entry.requestId) {
			entry.requestId = 0;
			if (const auto item = _session->data().message(id)) {
				if (!entry.pending) {
					toggleRound(item, entry);
				}
				_session->data().requestItemResize(item);
			}
		}
	}).fail([=](const Error &error) {
#if 0 // mtp
	const auto requestId = _api.request(MTPmessages_TranscribeAudio(
		item->history()->peer->input,
		MTP_int(item->id)
	)).done([=](const MTPmessages_TranscribedAudio &result) {
		const auto &data = result.data();

		{
			const auto trialsCountChanged = data.vtrial_remains_num()
				&& (_trialsCount != data.vtrial_remains_num()->v);
			if (trialsCountChanged) {
				_trialsCount = data.vtrial_remains_num()->v;
			}
			const auto refreshAtChanged = data.vtrial_remains_until_date()
				&& (_trialsRefreshAt != data.vtrial_remains_until_date()->v);
			if (refreshAtChanged) {
				_trialsRefreshAt = data.vtrial_remains_until_date()->v;
			}
			if (trialsCountChanged) {
				ShowTrialTranscribesToast(_trialsCount, _trialsRefreshAt);
			}
		}

		auto &entry = _map[id];
		entry.requestId = 0;
		entry.pending = data.is_pending();
		entry.result = qs(data.vtext());
		_ids.emplace(data.vtranscription_id().v, id);
		if (const auto item = _session->data().message(id)) {
			toggleRound(item, entry);
			_session->data().requestItemResize(item);
		}
	}).fail([=](const MTP::Error &error) {
#endif
		auto &entry = _map[id];
		entry.requestId = 0;
		entry.pending = false;
		entry.failed = true;
#if 0 // mtp
		if (error.type() == u"MSG_VOICE_TOO_LONG"_q) {
#endif
		if (error.message == u"MSG_VOICE_TOO_LONG"_q) {
			entry.toolong = true;
		}
		if (const auto item = _session->data().message(id)) {
			toggleRound(item, entry);
			_session->data().requestItemResize(item);
		}
	}).send();
	auto &entry = _map.emplace(id).first->second;
	entry.requestId = requestId;
	entry.shown = true;
	entry.failed = false;
#if 0 // mtp
	entry.pending = false;
#endif
	entry.pending = true;
}

} // namespace Api
