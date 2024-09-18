/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/components/factchecks.h"

#include "api/api_text_entities.h"
#include "apiwrap.h"
#include "base/random.h"
#include "data/data_session.h"
#include "data/data_web_page.h"
#include "history/view/media/history_view_web_page.h"
#include "history/view/history_view_message.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "lang/lang_keys.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "ui/layers/show.h"

#include "tdb/tdb_tl_scheme.h"
#include "tdb/tdb_sender.h"

namespace Data {
namespace {

using namespace Tdb;

constexpr auto kRequestDelay = crl::time(1000);

} // namespace

Factchecks::Factchecks(not_null<Main::Session*> session)
: _session(session)
#if 0 // mtp
, _requestTimer([=] { request(); }) {
#endif
{
}

#if 0 // mtp
void Factchecks::requestFor(not_null<HistoryItem*> item) {
	subscribeIfNotYet();

	if (const auto factcheck = item->Get<HistoryMessageFactcheck>()) {
		factcheck->requested = true;
	}
	if (!_requestTimer.isActive()) {
		_requestTimer.callOnce(kRequestDelay);
	}
	const auto changed = !_pending.empty()
		&& (_pending.front()->history() != item->history());
	const auto added = _pending.emplace(item).second;
	if (changed) {
		request();
	} else if (added && _pending.size() == 1) {
		_requestTimer.callOnce(kRequestDelay);
	}
}

void Factchecks::subscribeIfNotYet() {
	if (_subscribed) {
		return;
	}
	_subscribed = true;

	_session->data().itemRemoved(
	) | rpl::start_with_next([=](not_null<const HistoryItem*> item) {
		_pending.remove(item);
		const auto i = ranges::find(_requested, item.get());
		if (i != end(_requested)) {
			*i = nullptr;
		}
	}, _lifetime);
}

void Factchecks::request() {
	_requestTimer.cancel();

	if (!_requested.empty() || _pending.empty()) {
		return;
	}
	_session->api().request(base::take(_requestId)).cancel();

	auto ids = QVector<MTPint>();
	ids.reserve(_pending.size());
	const auto history = _pending.front()->history();
	for (auto i = begin(_pending); i != end(_pending);) {
		const auto &item = *i;
		if (item->history() == history) {
			_requested.push_back(item);
			ids.push_back(MTP_int(item->id.bare));
			i = _pending.erase(i);
		} else {
			++i;
		}
	}
	_requestId = _session->api().request(MTPmessages_GetFactCheck(
		history->peer->input,
		MTP_vector<MTPint>(std::move(ids))
	)).done([=](const MTPVector<MTPFactCheck> &result) {
		_requestId = 0;
		const auto &list = result.v;
		auto index = 0;
		for (const auto &item : base::take(_requested)) {
			if (!item) {
			} else if (index >= list.size()) {
				item->setFactcheck({});
			} else {
				item->setFactcheck(FromMTP(item, &list[index]));
			}
			++index;
		}
		if (!_pending.empty()) {
			request();
		}
	}).fail([=] {
		_requestId = 0;
		for (const auto &item : base::take(_requested)) {
			if (item) {
				item->setFactcheck({});
			}
		}
		if (!_pending.empty()) {
			request();
		}
	}).send();
}
#endif

void Factchecks::apply(const TLDupdateMessageFactCheck &data) {
	const auto fullId = FullMsgId{
		peerFromTdbChat(data.vchat_id()),
		data.vmessage_id().v
	};
	if (const auto item = _session->data().message(fullId)) {
		item->setFactcheck(FromTL(&data.vfact_check()));
	}
}

bool Factchecks::apply(const Tdb::TLDupdateOption &data) {
	if (data.vname().v == u"can_edit_fact_check"_q) {
		_canEdit = data.vvalue().c_optionValueBoolean().vvalue().v;
	} else if (data.vname().v == u"fact_check_length_max"_q) {
		_lengthLimit = data.vvalue().c_optionValueInteger().vvalue().v;
	} else {
		return false;
	}
	return true;
}

std::unique_ptr<HistoryView::WebPage> Factchecks::makeMedia(
		not_null<HistoryView::Message*> view,
		not_null<HistoryMessageFactcheck*> factcheck) {
	if (!factcheck->page) {
		factcheck->page = view->history()->owner().webpage(
			base::RandomValue<WebPageId>(),
			tr::lng_factcheck_title(tr::now),
			factcheck->data.text);
		factcheck->page->type = WebPageType::Factcheck;
	}
	return std::make_unique<HistoryView::WebPage>(
		view,
		factcheck->page,
		MediaWebPageFlags());
}

bool Factchecks::canEdit(not_null<HistoryItem*> item) const {
	if (!canEdit()
		|| !item->isRegular()
		|| !item->history()->peer->isBroadcast()) {
		return false;
	}
	const auto media = item->media();
	if (!media || media->webpage() || media->photo()) {
		return true;
	} else if (const auto document = media->document()) {
		return !document->isVideoMessage() && !document->sticker();
	}
	return false;
}

bool Factchecks::canEdit() const {
#if 0 // mtp
	return _session->appConfig().get<bool>(u"can_edit_factcheck"_q, false);
#endif
	return _canEdit;
}

int Factchecks::lengthLimit() const {
#if 0 // mtp
	return _session->appConfig().get<int>(u"factcheck_length_limit"_q, 1024);
#endif
	return _lengthLimit;
}

void Factchecks::save(
		FullMsgId itemId,
		TextWithEntities text,
		Fn<void(QString)> done) {
	const auto item = _session->data().message(itemId);
	if (!item) {
		return;
#if 0 // mtp
	} else if (text.empty()) {
		_session->api().request(MTPmessages_DeleteFactCheck(
			item->history()->peer->input,
			MTP_int(item->id.bare)
		)).done([=](const MTPUpdates &result) {
			_session->api().applyUpdates(result);
			done(QString());
		}).fail([=](const MTP::Error &error) {
			done(error.type());
		}).send();
	} else {
		_session->api().request(MTPmessages_EditFactCheck(
			item->history()->peer->input,
			MTP_int(item->id.bare),
			MTP_textWithEntities(
				MTP_string(text.text),
				Api::EntitiesToMTP(
					_session,
					text.entities,
					Api::ConvertOption::SkipLocal))
		)).done([=](const MTPUpdates &result) {
			_session->api().applyUpdates(result);
			done(QString());
		}).fail([=](const MTP::Error &error) {
			done(error.type());
		}).send();
#endif
	}
	_session->sender().request(TLsetMessageFactCheck(
		peerToTdbChat(item->history()->peer->id),
		tl_int53(item->id.bare),
		(text.empty()
			? std::optional<TLformattedText>()
			: Api::FormattedTextToTdb(text))
	)).done([=] {
		done(QString());
	}).fail([=](const Error &error) {
		done(error.message);
	}).send();
}

void Factchecks::save(
		FullMsgId itemId,
		const TextWithEntities &was,
		TextWithEntities text,
		std::shared_ptr<Ui::Show> show) {
	const auto wasEmpty = was.empty();
	const auto textEmpty = text.empty();
	save(itemId, std::move(text), [=](QString error) {
		show->showToast(!error.isEmpty()
			? error
			: textEmpty
			? tr::lng_factcheck_remove_done(tr::now)
			: wasEmpty
			? tr::lng_factcheck_add_done(tr::now)
			: tr::lng_factcheck_edit_done(tr::now));
	});
}

} // namespace Data
