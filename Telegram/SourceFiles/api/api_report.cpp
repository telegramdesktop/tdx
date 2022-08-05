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
#include "data/data_report.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/report_box_graphics.h"
#include "ui/layers/show.h"

#include "tdb/tdb_tl_scheme.h"
#include "tdb/tdb_sender.h"

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
		std::variant<v::null_t, not_null<PhotoData*>> data) {
	auto done = [=] {
		show->showToast(tr::lng_report_thanks(tr::now));
	};
	v::match(data, [&](v::null_t) {
		Unexpected("This won't be here.");
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
	});
#if 0 // mtp
	v::match(data, [&](v::null_t) {
		peer->session().api().request(MTPaccount_ReportPeer(
			peer->input,
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
	});
#endif
}

auto CreateReportMessagesOrStoriesCallback(
	std::shared_ptr<Ui::Show> show,
	not_null<PeerData*> peer)
-> Fn<void(Data::ReportInput, Fn<void(ReportResult)>)> {
	using TLChoose = MTPDreportResultChooseOption;
	using TLAddComment = MTPDreportResultAddComment;
	using TLReported = MTPDreportResultReported;
	using Result = ReportResult;

	struct State final {
#ifdef _DEBUG
		~State() {
			qDebug() << "Messages or Stories Report ~State().";
		}
#endif
		mtpRequestId requestId = 0;
	};
	const auto state = std::make_shared<State>();

	return [=](
			Data::ReportInput reportInput,
			Fn<void(Result)> done) {
#if 0 // mtp
		auto apiIds = QVector<MTPint>();
		apiIds.reserve(reportInput.ids.size() + reportInput.stories.size());
		for (const auto &id : reportInput.ids) {
			apiIds.push_back(MTP_int(id));
		}
		for (const auto &story : reportInput.stories) {
			apiIds.push_back(MTP_int(story));
		}

		const auto received = [=](
				const MTPReportResult &result,
				mtpRequestId requestId) {
			if (state->requestId != requestId) {
				return;
			}
			state->requestId = 0;
			done(result.match([&](const TLChoose &data) {
				const auto t = qs(data.vtitle());
				auto list = Result::Options();
				list.reserve(data.voptions().v.size());
				for (const auto &tl : data.voptions().v) {
					list.emplace_back(Result::Option{
						.id = tl.data().voption().v,
						.text = qs(tl.data().vtext()),
					});
				}
				return Result{ .options = std::move(list), .title = t };
			}, [&](const TLAddComment &data) -> Result {
				return {
					.commentOption = ReportResult::CommentOption{
						.optional = data.is_optional(),
						.id = data.voption().v,
					}
				};
			}, [&](const TLReported &data) -> Result {
				return { .successful = true };
			}));
		};

		const auto fail = [=](const MTP::Error &error) {
			state->requestId = 0;
			done({ .error = error.type() });
		};

		if (!reportInput.stories.empty()) {
			state->requestId = peer->session().api().request(
				MTPstories_Report(
					peer->input,
					MTP_vector<MTPint>(apiIds),
					MTP_bytes(reportInput.optionId),
					MTP_string(reportInput.comment))
			).done(received).fail(fail).send();
		} else {
			state->requestId = peer->session().api().request(
				MTPmessages_Report(
					peer->input,
					MTP_vector<MTPint>(apiIds),
					MTP_bytes(reportInput.optionId),
					MTP_string(reportInput.comment))
			).done(received).fail(fail).send();
		}
#endif
		const auto fail = [=](const Error &error) {
			state->requestId = 0;
			done({ .error = error.message });
		};
		const auto optionRequired = [=](const auto &data) {
			const auto t = data.vtitle().v;
			auto list = Result::Options();
			list.reserve(data.voptions().v.size());
			for (const auto &tl : data.voptions().v) {
				list.emplace_back(Result::Option{
					.id = tl.data().vid().v,
					.text = tl.data().vtext().v,
				});
			}
			return Result{ .options = std::move(list), .title = t };
		};
		const auto textRequired = [=](const auto &data) {
			return Result{
				.commentOption = ReportResult::CommentOption{
					.optional = data.vis_optional().v,
					.id = data.voption_id().v,
				},
			};
		};
		if (!reportInput.stories.empty()) {
			Assert(reportInput.stories.size() == 1);
			state->requestId = peer->session().sender().request(
				TLreportStory(
					peerToTdbChat(peer->id),
					tl_int32(reportInput.stories.front()),
					tl_bytes(reportInput.optionId),
					tl_string(reportInput.comment))
			).done([=](
					const TLreportStoryResult &result,
					mtpRequestId requestId) {
				if (state->requestId != requestId) {
					return;
				}
				state->requestId = 0;
				done(result.match([](const TLDreportStoryResultOk &) {
					return Result{ .successful = true };
				}, [&](const TLDreportStoryResultOptionRequired &data) {
					return optionRequired(data);
				}, [&](const TLDreportStoryResultTextRequired &data) {
					return textRequired(data);
				}));
			}).fail(fail).send();
		} else {
			state->requestId = peer->session().sender().request(
				TLreportChat(
					peerToTdbChat(peer->id),
					tl_bytes(reportInput.optionId),
					tl_vector<TLint53>(ranges::views::all(
						reportInput.ids
					) | ranges::views::transform([](MsgId id) {
						return tl_int53(id.bare);
					}) | ranges::to<QVector<TLint53>>()),
					tl_string(reportInput.comment))
			).done([=](
					const TLreportChatResult &result,
					mtpRequestId requestId) {
				if (state->requestId != requestId) {
					return;
				}
				state->requestId = 0;
				done(result.match([](const TLDreportChatResultOk &) {
					return Result{ .successful = true };
				}, [&](const TLDreportChatResultOptionRequired &data) {
					return optionRequired(data);
				}, [&](const TLDreportChatResultTextRequired &data) {
					return textRequired(data);
				}, [&](const TLDreportChatResultMessagesRequired &) {
					return Result{ .error = u"MESSAGE_ID_REQUIRED"_q };
				}));
			}).fail(fail).send();
		}
	};
}

} // namespace Api
