/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_drafts.h"

#include "api/api_text_entities.h"
#include "ui/widgets/fields/input_field.h"
#include "chat_helpers/message_field.h"
#include "history/history.h"
#include "history/history_widget.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "data/data_changes.h"
#include "data/data_session.h"
#include "data/data_web_page.h"
#include "mainwidget.h"
#include "storage/localstorage.h"

#include "tdb/tdb_tl_scheme.h"

namespace Data {
namespace {

using namespace Tdb;

[[nodiscard]] FullReplyTo ReplyToFromTL(
		not_null<History*> history,
		const TLinputMessageReplyTo *reply) {
	if (!reply) {
		return {};
	}
	return reply->match([&](const TLDinputMessageReplyToMessage &data) {
		auto result = FullReplyTo{
			.messageId = { history->peer->id, data.vmessage_id().v },
		};
		if (const auto quote = data.vquote()) {
			result.quote = Api::FormattedTextFromTdb(quote->data().vtext());
			result.quoteOffset = quote->data().vposition().v;
		}
		return result;
	}, [&](const TLDinputMessageReplyToExternalMessage &data) {
		auto result = FullReplyTo{
			.messageId = {
				peerFromTdbChat(data.vchat_id()),
				data.vmessage_id().v,
			},
		};
		if (const auto quote = data.vquote()) {
			result.quote = Api::FormattedTextFromTdb(quote->data().vtext());
			result.quoteOffset = quote->data().vposition().v;
		}
		return result;
	}, [&](const TLDinputMessageReplyToStory &data) {
		if (const auto id = peerFromTdbChat(data.vstory_sender_chat_id())) {
			return FullReplyTo{
				.storyId = { id, data.vstory_id().v },
			};
		}
		return FullReplyTo();
	});
}

} // namespace

WebPageDraft WebPageDraft::FromItem(not_null<HistoryItem*> item) {
	const auto previewMedia = item->media();
	const auto previewPage = previewMedia
		? previewMedia->webpage()
		: nullptr;
	using PageFlag = MediaWebPageFlag;
	const auto previewFlags = previewMedia
		? previewMedia->webpageFlags()
		: PageFlag();
	return {
		.id = previewPage ? previewPage->id : 0,
		.url = previewPage ? previewPage->url : QString(),
		.forceLargeMedia = !!(previewFlags & PageFlag::ForceLargeMedia),
		.forceSmallMedia = !!(previewFlags & PageFlag::ForceSmallMedia),
		.invert = item->invertMedia(),
		.manual = !!(previewFlags & PageFlag::Manual),
		.removed = !previewPage,
	};
}

Draft::Draft(
	const TextWithTags &textWithTags,
	FullReplyTo reply,
	const MessageCursor &cursor,
	WebPageDraft webpage,
	mtpRequestId saveRequestId)
: textWithTags(textWithTags)
, reply(std::move(reply))
, cursor(cursor)
, webpage(webpage)
, saveRequestId(saveRequestId) {
}

Draft::Draft(
	not_null<const Ui::InputField*> field,
	FullReplyTo reply,
	WebPageDraft webpage,
	mtpRequestId saveRequestId)
: textWithTags(field->getTextWithTags())
, reply(std::move(reply))
, cursor(field)
, webpage(webpage) {
}

#if 0 // goodToRemove
void ApplyPeerCloudDraft(
		not_null<Main::Session*> session,
		PeerId peerId,
		MsgId topicRootId,
		const MTPDdraftMessage &draft) {
	const auto history = session->data().history(peerId);
	const auto date = draft.vdate().v;
	if (history->skipCloudDraftUpdate(topicRootId, date)) {
		return;
	}
	const auto textWithTags = TextWithTags{
		qs(draft.vmessage()),
		TextUtilities::ConvertEntitiesToTextTags(
			Api::EntitiesFromMTP(
				session,
				draft.ventities().value_or_empty()))
	};
	auto replyTo = draft.vreply_to()
		? ReplyToFromMTP(history, *draft.vreply_to())
		: FullReplyTo();
	replyTo.topicRootId = topicRootId;
	auto webpage = WebPageDraft{
		.invert = draft.is_invert_media(),
		.removed = draft.is_no_webpage(),
	};
	if (const auto media = draft.vmedia()) {
		media->match([&](const MTPDmessageMediaWebPage &data) {
			const auto parsed = session->data().processWebpage(
				data.vwebpage());
			if (!parsed->failed) {
				webpage.forceLargeMedia = data.is_force_large_media();
				webpage.forceSmallMedia = data.is_force_small_media();
				webpage.manual = data.is_manual();
				webpage.url = parsed->url;
				webpage.id = parsed->id;
			}
		}, [](const auto &) {});
	}
	auto cloudDraft = std::make_unique<Draft>(
		textWithTags,
		replyTo,
		MessageCursor(Ui::kQFixedMax, Ui::kQFixedMax, Ui::kQFixedMax),
		std::move(webpage));
	cloudDraft->date = date;

	history->setCloudDraft(std::move(cloudDraft));
	history->applyCloudDraft(topicRootId);
}
#endif

void ApplyPeerCloudDraft(
		not_null<Main::Session*> session,
		PeerId peerId,
		MsgId topicRootId,
		const Tdb::TLDdraftMessage &draft) {
	draft.vinput_message_text().match([&](const Tdb::TLDinputMessageText &d) {
		const auto history = session->data().history(peerId);
		const auto date = draft.vdate().v;
		if (history->skipCloudDraftUpdate(topicRootId, date)) {
			return;
		}
		const auto text = Api::FormattedTextFromTdb(d.vtext());
		const auto textWithTags = TextWithTags{
			text.text,
			TextUtilities::ConvertEntitiesToTextTags(text.entities)
		};
		auto replyTo = ReplyToFromTL(history, draft.vreply_to());
		replyTo.topicRootId = topicRootId;
		auto webpage = WebPageDraft();
		if (const auto options = d.vlink_preview_options()) {
			const auto &fields = options->data();
			webpage.removed = fields.vis_disabled().v;
			webpage.invert = fields.vshow_above_text().v;
			webpage.forceLargeMedia = fields.vforce_large_media().v;
			webpage.forceSmallMedia = fields.vforce_small_media().v;
			webpage.url = fields.vurl().v;
		};
		auto cloudDraft = std::make_unique<Draft>(
			textWithTags,
			replyTo,
			MessageCursor(Ui::kQFixedMax, Ui::kQFixedMax, Ui::kQFixedMax),
			std::move(webpage));
		cloudDraft->date = date;

		history->setCloudDraft(std::move(cloudDraft));
		history->applyCloudDraft(topicRootId);
	}, [](const auto &) {
		Unexpected("Unsupported draft content type.");
	});
}

void ClearPeerCloudDraft(
		not_null<Main::Session*> session,
		PeerId peerId,
		MsgId topicRootId,
		TimeId date) {
	const auto history = session->data().history(peerId);
	if (history->skipCloudDraftUpdate(topicRootId, date)) {
		return;
	}

	history->clearCloudDraft(topicRootId);
	history->applyCloudDraft(topicRootId);
}

void SetChatLinkDraft(not_null<PeerData*> peer, TextWithEntities draft) {
	static const auto kInlineStart = QRegularExpression("^@[a-zA-Z0-9_]");
	if (kInlineStart.match(draft.text).hasMatch()) {
		draft = TextWithEntities().append(' ').append(std::move(draft));
	}

	const auto textWithTags = TextWithTags{
		draft.text,
		TextUtilities::ConvertEntitiesToTextTags(draft.entities)
	};
	const auto cursor = MessageCursor{
		int(textWithTags.text.size()),
		int(textWithTags.text.size()),
		Ui::kQFixedMax
	};
	const auto history = peer->owner().history(peer->id);
	const auto topicRootId = MsgId();
	history->setLocalDraft(std::make_unique<Data::Draft>(
		textWithTags,
		FullReplyTo{ .topicRootId = topicRootId },
		cursor,
		Data::WebPageDraft()));
	history->clearLocalEditDraft(topicRootId);
	history->session().changes().entryUpdated(
		history,
		Data::EntryUpdate::Flag::LocalDraftSet);
}

std::optional<TLlinkPreviewOptions> LinkPreviewOptions(
		const WebPageDraft &webpage) {
	return (webpage.removed || !webpage.url.isEmpty())
		? tl_linkPreviewOptions(
			tl_bool(webpage.removed),
			tl_string(webpage.url),
			tl_bool(webpage.forceSmallMedia),
			tl_bool(webpage.forceLargeMedia),
			tl_bool(webpage.invert))
		: std::optional<TLlinkPreviewOptions>();
}

} // namespace Data
