/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_report.h"

#include "apiwrap.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/report_box.h"
#include "ui/layers/show.h"

namespace Api {

namespace {

using namespace Tdb;

[[nodiscard]] TLreportReason ReasonToTL(const Ui::ReportReason &reason) {
	using Reason = Ui::ReportReason;
	switch (reason) {
	case Reason::Spam: return tl_reportReasonSpam();
	case Reason::Fake: return tl_reportReasonFake();
	case Reason::Violence: return tl_reportReasonViolence();
	case Reason::ChildAbuse: return tl_reportReasonChildAbuse();
	case Reason::Pornography: return tl_reportReasonPornography();
	case Reason::Copyright: return tl_reportReasonCopyright();
	case Reason::IllegalDrugs: return tl_reportReasonIllegalDrugs();
	case Reason::PersonalDetails: return tl_reportReasonPersonalDetails();
	case Reason::Other: return tl_reportReasonCustom();
	}
	Unexpected("Bad reason group value.");
}
#if 0 // mtp
MTPreportReason ReasonToTL(const Ui::ReportReason &reason) {
	using Reason = Ui::ReportReason;
	switch (reason) {
	case Reason::Spam: return MTP_inputReportReasonSpam();
	case Reason::Fake: return MTP_inputReportReasonFake();
	case Reason::Violence: return MTP_inputReportReasonViolence();
	case Reason::ChildAbuse: return MTP_inputReportReasonChildAbuse();
	case Reason::Pornography: return MTP_inputReportReasonPornography();
	case Reason::Copyright: return MTP_inputReportReasonCopyright();
	case Reason::IllegalDrugs: return MTP_inputReportReasonIllegalDrugs();
	case Reason::PersonalDetails:
		return MTP_inputReportReasonPersonalDetails();
	case Reason::Other: return MTP_inputReportReasonOther();
	}
	Unexpected("Bad reason group value.");
}
#endif

} // namespace

void SendReport(
	std::shared_ptr<Ui::Show> show,
	not_null<PeerData*> peer,
	Ui::ReportReason reason,
	const QString &comment,
	std::variant<
		v::null_t,
		MessageIdsList,
		not_null<PhotoData*>,
		StoryId> data) {
	auto done = [=] {
		show->showToast(tr::lng_report_thanks(tr::now));
	};
	v::match(data, [&](v::null_t) {
		peer->session().sender().request(TLreportChat(
			peerToTdbChat(peer->id),
			tl_vector<TLint53>(),
			ReasonToTL(reason),
			tl_string(comment)
		)).done(std::move(done)).send();
	}, [&](const MessageIdsList &ids) {
		auto list = QVector<TLint53>();
		list.reserve(ids.size());
		for (const auto &fullId : ids) {
			list.push_back(tl_int53(fullId.msg.bare));
		}
		peer->session().sender().request(TLreportChat(
			peerToTdbChat(peer->id),
			tl_vector<TLint53>(list),
			ReasonToTL(reason),
			tl_string(comment)
		)).done(std::move(done)).send();
	}, [&](not_null<PhotoData*> photo) {
		const auto tdb = std::get_if<TdbFileLocation>(
			&photo->location(Data::PhotoSize::Large).file().data);
		if (tdb) {
			peer->session().sender().request(TLreportChatPhoto(
				peerToTdbChat(peer->id),
				tl_int32(tdb->fileId),
				ReasonToTL(reason),
				tl_string(comment)
			)).done(std::move(done)).send();
		}
	}, [&](StoryId id) {
		peer->session().sender().request(TLreportStory(
			peerToTdbChat(peer->id),
			tl_int32(id),
			ReasonToTL(reason),
			tl_string(comment)
		)).done(std::move(done)).send();
	});
#if 0 // mtp
	v::match(data, [&](v::null_t) {
		peer->session().api().request(MTPaccount_ReportPeer(
			peer->input,
			ReasonToTL(reason),
			MTP_string(comment)
		)).done(std::move(done)).send();
	}, [&](const MessageIdsList &ids) {
		auto apiIds = QVector<MTPint>();
		apiIds.reserve(ids.size());
		for (const auto &fullId : ids) {
			apiIds.push_back(MTP_int(fullId.msg));
		}
		peer->session().api().request(MTPmessages_Report(
			peer->input,
			MTP_vector<MTPint>(apiIds),
			ReasonToTL(reason),
			MTP_string(comment)
		)).done(std::move(done)).send();
	}, [&](not_null<PhotoData*> photo) {
		peer->session().api().request(MTPaccount_ReportProfilePhoto(
			peer->input,
			photo->mtpInput(),
			ReasonToTL(reason),
			MTP_string(comment)
		)).done(std::move(done)).send();
	}, [&](StoryId id) {
		peer->session().api().request(MTPstories_Report(
			peer->input,
			MTP_vector<MTPint>(1, MTP_int(id)),
			ReasonToTL(reason),
			MTP_string(comment)
		)).done(std::move(done)).send();
	});
#endif
}

} // namespace Api
