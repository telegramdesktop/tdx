/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_sending.h"

#include "api/api_text_entities.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "data/business/data_shortcut_messages.h"
#include "data/data_document.h"
#include "data/data_photo.h"
#include "data/data_channel.h" // ChannelData::addsSignature.
#include "data/data_user.h" // UserData::name
#include "data/data_session.h"
#include "data/data_file_origin.h"
#include "data/data_histories.h"
#include "data/data_changes.h"
#include "data/stickers/data_stickers.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h" // NewMessageFlags.
#include "chat_helpers/message_field.h" // ConvertTextTagsToEntities.
#include "chat_helpers/stickers_dice_pack.h" // DicePacks::kDiceString.
#include "ui/text/text_entity.h" // TextWithEntities.
#include "ui/item_text_options.h" // Ui::ItemTextOptions.
#include "main/main_session.h"
#include "main/main_app_config.h"
#include "storage/localimageloader.h"
#include "storage/file_upload.h"
#include "mainwidget.h"
#include "apiwrap.h"

#include "data/components/scheduled_messages.h"
#include "tdb/tdb_file_generator.h"
#include "tdb/tdb_tl_scheme.h"

namespace Api {
namespace {

using namespace Tdb;

constexpr auto kScheduledTillOnline = Api::kScheduledUntilOnlineTimestamp;

void InnerFillMessagePostFlags(
		const SendOptions &options,
		not_null<PeerData*> peer,
		MessageFlags &flags) {
	if (ShouldSendSilent(peer, options)) {
		flags |= MessageFlag::Silent;
	}
	if (!peer->amAnonymous()
		|| (!peer->isBroadcast()
			&& options.sendAs
			&& options.sendAs != peer)) {
		flags |= MessageFlag::HasFromId;
	}
	const auto channel = peer->asBroadcast();
	if (!channel) {
		return;
	}
	flags |= MessageFlag::Post;
	// Don't display views and author of a new post when it's scheduled.
	if (options.scheduled) {
		return;
	}
	flags |= MessageFlag::HasViews;
	if (channel->addsSignature()) {
		flags |= MessageFlag::HasPostAuthor;
	}
}

#if 0 // mtp
void SendSimpleMedia(SendAction action, MTPInputMedia inputMedia) {
	const auto history = action.history;
	const auto peer = history->peer;
	const auto session = &history->session();
	const auto api = &session->api();

	action.clearDraft = false;
	action.generateLocal = false;
	api->sendAction(action);

	const auto randomId = base::RandomValue<uint64>();

	auto flags = NewMessageFlags(peer);
	auto sendFlags = MTPmessages_SendMedia::Flags(0);
	if (action.replyTo) {
		flags |= MessageFlag::HasReplyInfo;
		sendFlags |= MTPmessages_SendMedia::Flag::f_reply_to;
	}
	const auto silentPost = ShouldSendSilent(peer, action.options);
	InnerFillMessagePostFlags(action.options, peer, flags);
	if (silentPost) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_silent;
	}
	const auto sendAs = action.options.sendAs;
	if (sendAs) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_send_as;
	}
	const auto messagePostAuthor = peer->isBroadcast()
		? session->user()->name()
		: QString();

	if (action.options.scheduled) {
		flags |= MessageFlag::IsOrWasScheduled;
		sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_date;
	}
	if (action.options.shortcutId) {
		flags |= MessageFlag::ShortcutMessage;
		sendFlags |= MTPmessages_SendMedia::Flag::f_quick_reply_shortcut;
	}
	if (action.options.effectId) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_effect;
	}
	if (action.options.invertCaption) {
		flags |= MessageFlag::InvertMedia;
		sendFlags |= MTPmessages_SendMedia::Flag::f_invert_media;
	}

	auto &histories = history->owner().histories();
	histories.sendPreparedMessage(
		history,
		action.replyTo,
		randomId,
		Data::Histories::PrepareMessage<MTPmessages_SendMedia>(
			MTP_flags(sendFlags),
			peer->input,
			Data::Histories::ReplyToPlaceholder(),
			std::move(inputMedia),
			MTPstring(),
			MTP_long(randomId),
			MTPReplyMarkup(),
			MTPvector<MTPMessageEntity>(),
			MTP_int(action.options.scheduled),
			(sendAs ? sendAs->input : MTP_inputPeerEmpty()),
			Data::ShortcutIdToMTP(session, action.options.shortcutId),
			MTP_long(action.options.effectId)
		), [=](const MTPUpdates &result, const MTP::Response &response) {
	}, [=](const MTP::Error &error, const MTP::Response &response) {
		api->sendMessageFail(error, peer, randomId);
	});

	api->finishForwarding(action);
}

template <typename MediaData>
void SendExistingMedia(
		MessageToSend &&message,
		not_null<MediaData*> media,
		Fn<MTPInputMedia()> inputMedia,
		Data::FileOrigin origin,
		std::optional<MsgId> localMessageId) {
	const auto history = message.action.history;
	const auto peer = history->peer;
	const auto session = &history->session();
	const auto api = &session->api();

	message.action.clearDraft = false;
	message.action.generateLocal = true;
	api->sendAction(message.action);

	const auto newId = FullMsgId(
		peer->id,
		localMessageId
			? (*localMessageId)
			: session->data().nextLocalMessageId());
	const auto randomId = base::RandomValue<uint64>();
	const auto &action = message.action;

	auto flags = NewMessageFlags(peer);
	auto sendFlags = MTPmessages_SendMedia::Flags(0);
	if (action.replyTo) {
		flags |= MessageFlag::HasReplyInfo;
		sendFlags |= MTPmessages_SendMedia::Flag::f_reply_to;
	}
	const auto silentPost = ShouldSendSilent(peer, action.options);
	InnerFillMessagePostFlags(action.options, peer, flags);
	if (silentPost) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_silent;
	}
	const auto sendAs = action.options.sendAs;
	if (sendAs) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_send_as;
	}
	auto caption = TextWithEntities{
		message.textWithTags.text,
		TextUtilities::ConvertTextTagsToEntities(message.textWithTags.tags)
	};
	TextUtilities::Trim(caption);
	auto sentEntities = EntitiesToMTP(
		session,
		caption.entities,
		ConvertOption::SkipLocal);
	if (!sentEntities.v.isEmpty()) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_entities;
	}
	const auto captionText = caption.text;

	if (action.options.scheduled) {
		flags |= MessageFlag::IsOrWasScheduled;
		sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_date;
	}
	if (action.options.shortcutId) {
		flags |= MessageFlag::ShortcutMessage;
		sendFlags |= MTPmessages_SendMedia::Flag::f_quick_reply_shortcut;
	}
	if (action.options.effectId) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_effect;
	}
	if (action.options.invertCaption) {
		flags |= MessageFlag::InvertMedia;
		sendFlags |= MTPmessages_SendMedia::Flag::f_invert_media;
	}

	session->data().registerMessageRandomId(randomId, newId);

	history->addNewLocalMessage({
		.id = newId.msg,
		.flags = flags,
		.from = NewMessageFromId(action),
		.replyTo = action.replyTo,
		.date = NewMessageDate(action.options),
		.shortcutId = action.options.shortcutId,
		.postAuthor = NewMessagePostAuthor(action),
		.effectId = action.options.effectId,
	}, media, caption);

	const auto performRequest = [=](const auto &repeatRequest) -> void {
		auto &histories = history->owner().histories();
		const auto session = &history->session();
		const auto usedFileReference = media->fileReference();
		histories.sendPreparedMessage(
			history,
			action.replyTo,
			randomId,
			Data::Histories::PrepareMessage<MTPmessages_SendMedia>(
				MTP_flags(sendFlags),
				peer->input,
				Data::Histories::ReplyToPlaceholder(),
				inputMedia(),
				MTP_string(captionText),
				MTP_long(randomId),
				MTPReplyMarkup(),
				sentEntities,
				MTP_int(action.options.scheduled),
				(sendAs ? sendAs->input : MTP_inputPeerEmpty()),
				Data::ShortcutIdToMTP(session, action.options.shortcutId),
				MTP_long(action.options.effectId)
			), [=](const MTPUpdates &result, const MTP::Response &response) {
		}, [=](const MTP::Error &error, const MTP::Response &response) {
			if (error.code() == 400
				&& error.type().startsWith(u"FILE_REFERENCE_"_q)) {
				api->refreshFileReference(origin, [=](const auto &result) {
					if (media->fileReference() != usedFileReference) {
						repeatRequest(repeatRequest);
					} else {
						api->sendMessageFail(error, peer, randomId, newId);
					}
				});
			} else {
				api->sendMessageFail(error, peer, randomId, newId);
			}
		});
	};
	performRequest(performRequest);

	api->finishForwarding(action);
}
#endif

[[nodiscard]] TLinputMessageContent MessageContentFromFile(
		not_null<Main::Session*> session,
		const std::shared_ptr<FilePrepareResult> &file,
		const Storage::ReadyFileWithThumbnail &ready) {
	Expects(ready.file != nullptr);

	auto caption = TextWithEntities{
		file->caption.text,
		TextUtilities::ConvertTextTagsToEntities(file->caption.tags)
	};
	TextUtilities::PrepareForSending(caption, 0);
	TextUtilities::Trim(caption);
	const auto formatted = caption.text.isEmpty()
		? std::optional<TLformattedText>()
		: Api::FormattedTextToTdb(caption);

	const auto &fields = ready.file->data();
	const auto thumbnail = (ready.thumbnailGenerator
		? tl_inputThumbnail(
			ready.thumbnailGenerator->inputFile(),
			tl_int32(file->thumbnailDimensions.width()),
			tl_int32(file->thumbnailDimensions.height()))
		: std::optional<TLinputThumbnail>());
	const auto fileById = tl_inputFileId(tl_int32(fields.vid().v));
	const auto seconds = file->duration / 1000;
	const auto attached = tl_vector<TLint32>(file->attachedStickers
		| ranges::views::transform(tl_int32)
		| ranges::to<QVector>());
	switch (file->filetype) {
	case PreparedFileType::Photo:
		return tl_inputMessagePhoto(
			fileById,
			thumbnail,
			attached,
			tl_int32(file->dimensions.width()),
			tl_int32(file->dimensions.height()),
			formatted,
			tl_bool(file->to.options.invertCaption),
			null, // self_destruct_type
			tl_bool(file->spoiler));
	case PreparedFileType::Animation:
		return tl_inputMessageAnimation(
			fileById,
			thumbnail,
			attached,
			tl_int32(seconds),
			tl_int32(file->dimensions.width()),
			tl_int32(file->dimensions.height()),
			formatted,
			tl_bool(file->to.options.invertCaption),
			tl_bool(file->spoiler));
	case PreparedFileType::Audio:
		return tl_inputMessageAudio(
			fileById,
			thumbnail,
			tl_int32(seconds),
			tl_string(file->title),
			tl_string(file->performer),
			formatted);
	case PreparedFileType::Document:
		return tl_inputMessageDocument(
			fileById,
			thumbnail,
			tl_bool(false),
			formatted);
	case PreparedFileType::Sticker:
		return tl_inputMessageSticker(
			fileById,
			thumbnail,
			tl_int32(file->dimensions.width()),
			tl_int32(file->dimensions.height()),
			tl_string()); // Emoji used for search.
	case PreparedFileType::Video:
		return tl_inputMessageVideo(
			fileById,
			thumbnail,
			tl_vector<TLint32>(),
			tl_int32(seconds),
			tl_int32(file->dimensions.width()),
			tl_int32(file->dimensions.height()),
			tl_bool(file->supportsStreaming),
			formatted,
			tl_bool(file->to.options.invertCaption),
			null, // self_destruct_type
			tl_bool(file->spoiler));
	case PreparedFileType::VoiceNote:
		return tl_inputMessageVoiceNote(
			fileById,
			tl_int32(seconds),
			tl_bytes(file->waveform),
			formatted,
			null); // self_destruct_type
	}
	Unexpected("FilePrepareResult::filetype.");
}

[[nodiscard]] TLinputMessageContent MessageContentFromExisting(
		not_null<DocumentData*> document,
		TextWithTags &&text) {
	Expects(document->tdbFileId() != 0);

	auto caption = TextWithEntities{
		text.text,
		TextUtilities::ConvertTextTagsToEntities(text.tags)
	};
	TextUtilities::PrepareForSending(caption, 0);
	TextUtilities::Trim(caption);
	const auto formatted = caption.text.isEmpty()
		? std::optional<TLformattedText>()
		: Api::FormattedTextToTdb(caption);

	const auto thumbnail = std::optional<TLinputThumbnail>();
	const auto fileById = tl_inputFileId(tl_int32(document->tdbFileId()));
	if (document->isVideoMessage()) {
		return tl_inputMessageVideoNote(
			fileById,
			thumbnail,
			tl_int32(document->duration() / 1000),
			tl_int32(document->dimensions.width()),
			null); // self_destruct_type
	} else if (document->isVoiceMessage()) {
		return tl_inputMessageVoiceNote(
			fileById,
			tl_int32(document->duration() / 1000),
			tl_bytes(documentWaveformEncode5bit(document->voice()->waveform)),
			formatted,
			null); // self_destruct_type
	} else if (document->isAnimation()) {
		return tl_inputMessageAnimation(
			fileById,
			thumbnail,
			tl_vector<TLint32>(),
			tl_int32(document->duration() / 1000),
			tl_int32(document->dimensions.width()),
			tl_int32(document->dimensions.height()),
			formatted,
			tl_bool(false), // show_caption_above_media
			tl_bool(false)); // has_spoiler
	} else if (document->sticker()) {
		return tl_inputMessageSticker(
			fileById,
			thumbnail,
			tl_int32(document->dimensions.width()),
			tl_int32(document->dimensions.height()),
			tl_string()); // Emoji used for search.
	} else if (document->isVideoFile()) {
		return tl_inputMessageVideo(
			fileById,
			thumbnail,
			tl_vector<TLint32>(),
			tl_int32(document->duration() / 1000),
			tl_int32(document->dimensions.width()),
			tl_int32(document->dimensions.height()),
			tl_bool(document->supportsStreaming()),
			formatted,
			tl_bool(false), // show_caption_above_media
			null, // self_destruct_type
			tl_bool(false)); // has_spoiler
	} else if (const auto song = document->song()) {
		return tl_inputMessageAudio(
			fileById,
			thumbnail,
			tl_int32(document->duration() / 1000),
			tl_string(song->title),
			tl_string(song->performer),
			formatted);
	} else {
		return tl_inputMessageDocument(
			fileById,
			thumbnail,
			tl_bool(false),
			formatted);
	}
}

[[nodiscard]] TLinputMessageContent MessageContentFromExisting(
		not_null<PhotoData*> photo,
		TextWithTags &&text) {
	Expects(v::is<TdbFileLocation>(
		photo->location(Data::PhotoSize::Large).file().data));

	auto caption = TextWithEntities{
		text.text,
		TextUtilities::ConvertTextTagsToEntities(text.tags)
	};
	TextUtilities::PrepareForSending(caption, 0);
	TextUtilities::Trim(caption);

	const auto formatted = caption.text.isEmpty()
		? std::optional<TLformattedText>()
		: Api::FormattedTextToTdb(caption);

	const auto thumbnail = std::optional<TLinputThumbnail>();
	const auto fileById = tl_inputFileId(tl_int32(v::get<TdbFileLocation>(
		photo->location(Data::PhotoSize::Large).file().data).fileId));
	return tl_inputMessagePhoto(
		fileById,
		thumbnail,
		tl_vector<TLint32>(),
		tl_int32(photo->width()),
		tl_int32(photo->height()),
		formatted,
		tl_bool(false), // show_caption_above_media
		null, // self_destruct_type
		tl_bool(false)); // spoiler
}

void SendPreparedAlbumIfReady(
		const SendAction &action,
		not_null<SendingAlbum*> album) {
	if (album->items.empty()) {
		return;
	} else if (album->items.size() == 1) {
		if (const auto &content = album->items.front().content) {
			SendPreparedMessage(action, *content);
		}
		return;
	}
	auto contents = QVector<TLinputMessageContent>();
	contents.reserve(album->items.size());
	for (const auto &item : album->items) {
		if (!item.content) {
			return;
		}
		contents.push_back(*item.content);
	}
	const auto history = action.history;
	const auto peer = history->peer;
	const auto silentPost = ShouldSendSilent(peer, action.options);
	const auto session = &peer->session();
	session->sender().request(TLsendMessageAlbum(
		peerToTdbChat(peer->id),
		tl_int53(action.replyTo.topicRootId.bare),
		MessageReplyTo(action),
		tl_messageSendOptions(
			tl_bool(silentPost),
			tl_bool(false), // from_background
			tl_bool(false), // update_order_of_installed_stickers_sets
			ScheduledToTL(action.options.scheduled),
			tl_int64(action.options.effectId),
			tl_int32(0), // sending_id
			tl_bool(false)), // only_preview
		tl_vector(std::move(contents))
	)).done([=](const TLmessages &result) {
		// They should've been added by updates.
	}).fail([=](const Error &error) {
		const auto code = error.code;
		//if (error.type() == qstr("MESSAGE_EMPTY")) {
		//	lastMessage->destroy();
		//} else {
		//	sendMessageFail(error, peer, randomId, newId);
		//}
	}).send();
}

} // namespace

void SendExistingDocument(
		MessageToSend &&message,
		not_null<DocumentData*> document,
		std::optional<MsgId> localMessageId) {
	SendPreparedMessage(
		message.action,
		MessageContentFromExisting(
			document,
			std::move(message.textWithTags)),
		localMessageId);
#if 0 // mtp
	const auto inputMedia = [=] {
		return MTP_inputMediaDocument(
			MTP_flags(0),
			document->mtpInput(),
			MTPint(), // ttl_seconds
			MTPstring()); // query
	};
	SendExistingMedia(
		std::move(message),
		document,
		inputMedia,
		document->stickerOrGifOrigin(),
		std::move(localMessageId));
#endif

	if (document->sticker()) {
		document->owner().stickers().incrementSticker(document);
	}
}

void SendExistingPhoto(
		MessageToSend &&message,
		not_null<PhotoData*> photo,
		std::optional<MsgId> localMessageId) {
	SendPreparedMessage(
		message.action,
		MessageContentFromExisting(photo, std::move(message.textWithTags)),
		localMessageId);
#if 0 // mtp
	const auto inputMedia = [=] {
		return MTP_inputMediaPhoto(
			MTP_flags(0),
			photo->mtpInput(),
			MTPint());
	};
	SendExistingMedia(
		std::move(message),
		photo,
		inputMedia,
		Data::FileOrigin(),
		std::move(localMessageId));
#endif
}

bool SendDice(MessageToSend &message) {
	const auto full = QStringView(message.textWithTags.text).trimmed();
	auto length = 0;
	if (!Ui::Emoji::Find(full.data(), full.data() + full.size(), &length)
		|| length != full.size()
		|| !message.textWithTags.tags.isEmpty()) {
		return false;
	}
	auto &config = message.action.history->session().appConfig();
	static const auto hardcoded = std::vector<QString>{
		Stickers::DicePacks::kDiceString,
		Stickers::DicePacks::kDartString,
		Stickers::DicePacks::kSlotString,
		Stickers::DicePacks::kFballString,
		Stickers::DicePacks::kFballString + QChar(0xFE0F),
		Stickers::DicePacks::kBballString,
	};
#if 0 // mtp
	const auto list = config.get<std::vector<QString>>(
		"emojies_send_dice",
		hardcoded);
#endif
	const auto &pack = message.action.history->session().diceStickersPacks();
	const auto &list = pack.cloudDiceEmoticons();
	const auto emoji = full.toString();
	if (!ranges::contains(list.empty() ? hardcoded : list, emoji)) {
		return false;
	}
	const auto history = message.action.history;
	const auto peer = history->peer;
	const auto session = &history->session();
	const auto api = &session->api();

	message.textWithTags = TextWithTags();
	message.action.clearDraft = false;
	message.action.generateLocal = true;


	const auto &action = message.action;
	api->sendAction(action);

	session->sender().request(TLsendMessage(
		peerToTdbChat(peer->id),
		tl_int53(action.replyTo.topicRootId.bare),
		MessageReplyTo(action),
		MessageSendOptions(peer, action),
		tl_inputMessageDice(tl_string(emoji), tl_bool(action.clearDraft))
	)).fail([=](const Error &error) {
		const auto code = error.code;
		//if (error.type() == qstr("MESSAGE_EMPTY")) {
		//	lastMessage->destroy();
		//} else {
		//	sendMessageFail(error, peer, randomId, newId);
		//}
	}).send();

#if 0 // mtp
	const auto newId = FullMsgId(
		peer->id,
		session->data().nextLocalMessageId());
	const auto randomId = base::RandomValue<uint64>();

	auto &histories = history->owner().histories();
	auto flags = NewMessageFlags(peer);
	auto sendFlags = MTPmessages_SendMedia::Flags(0);
	if (action.replyTo) {
		flags |= MessageFlag::HasReplyInfo;
		sendFlags |= MTPmessages_SendMedia::Flag::f_reply_to;
	}
	const auto silentPost = ShouldSendSilent(peer, action.options);
	InnerFillMessagePostFlags(action.options, peer, flags);
	if (silentPost) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_silent;
	}
	const auto sendAs = action.options.sendAs;
	if (sendAs) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_send_as;
	}
	if (action.options.scheduled) {
		flags |= MessageFlag::IsOrWasScheduled;
		sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_date;
	}
	if (action.options.shortcutId) {
		flags |= MessageFlag::ShortcutMessage;
		sendFlags |= MTPmessages_SendMedia::Flag::f_quick_reply_shortcut;
	}
	if (action.options.effectId) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_effect;
	}
	if (action.options.invertCaption) {
		flags |= MessageFlag::InvertMedia;
		sendFlags |= MTPmessages_SendMedia::Flag::f_invert_media;
	}

	session->data().registerMessageRandomId(randomId, newId);

	history->addNewLocalMessage({
		.id = newId.msg,
		.flags = flags,
		.from = NewMessageFromId(action),
		.replyTo = action.replyTo,
		.date = NewMessageDate(action.options),
		.shortcutId = action.options.shortcutId,
		.postAuthor = NewMessagePostAuthor(action),
		.effectId = action.options.effectId,
	}, TextWithEntities(), MTP_messageMediaDice(
		MTP_int(0),
		MTP_string(emoji)));
	histories.sendPreparedMessage(
		history,
		action.replyTo,
		randomId,
		Data::Histories::PrepareMessage<MTPmessages_SendMedia>(
			MTP_flags(sendFlags),
			peer->input,
			Data::Histories::ReplyToPlaceholder(),
			MTP_inputMediaDice(MTP_string(emoji)),
			MTP_string(),
			MTP_long(randomId),
			MTPReplyMarkup(),
			MTP_vector<MTPMessageEntity>(),
			MTP_int(action.options.scheduled),
			(sendAs ? sendAs->input : MTP_inputPeerEmpty()),
			Data::ShortcutIdToMTP(session, action.options.shortcutId),
			MTP_long(action.options.effectId)
		), [=](const MTPUpdates &result, const MTP::Response &response) {
	}, [=](const MTP::Error &error, const MTP::Response &response) {
		api->sendMessageFail(error, peer, randomId, newId);
	});
	api->finishForwarding(action);
#endif

	return true;
}

void SendLocation(SendAction action, float64 lat, float64 lon) {
	SendSimpleMedia(
		action,
		MTP_inputMediaGeoPoint(
			MTP_inputGeoPoint(
				MTP_flags(0),
				MTP_double(lat),
				MTP_double(lon),
				MTPint()))); // accuracy_radius
}

void SendVenue(SendAction action, Data::InputVenue venue) {
	SendSimpleMedia(
		action,
		MTP_inputMediaVenue(
			MTP_inputGeoPoint(
				MTP_flags(0),
				MTP_double(venue.lat),
				MTP_double(venue.lon),
				MTPint()), // accuracy_radius
			MTP_string(venue.title),
			MTP_string(venue.address),
			MTP_string(venue.provider),
			MTP_string(venue.id),
			MTP_string(venue.venueType)));
}

void FillMessagePostFlags(
		const SendAction &action,
		not_null<PeerData*> peer,
		MessageFlags &flags) {
	InnerFillMessagePostFlags(action.options, peer, flags);
}

void SendConfirmedFile(
		not_null<Main::Session*> session,
		const std::shared_ptr<FilePrepareResult> &file) {
#if 0 // mtp
	const auto isEditing = (file->type != SendMediaType::Audio)
		&& (file->to.replaceMediaOf != 0);
	const auto newId = FullMsgId(
		file->to.peer,
		(isEditing
			? file->to.replaceMediaOf
			: session->data().nextLocalMessageId()));
	const auto groupId = file->album ? file->album->groupId : uint64(0);
	if (file->album) {
		const auto proj = [](const SendingAlbum::Item &item) {
			return item.taskId;
		};
		const auto it = ranges::find(file->album->items, file->taskId, proj);
		Assert(it != file->album->items.end());

		it->msgId = newId;
	}

	const auto itemToEdit = isEditing
		? session->data().message(newId)
		: nullptr;
	const auto history = session->data().history(file->to.peer);
	const auto peer = history->peer;

	if (!isEditing) {
		const auto histories = &session->data().histories();
		file->to.replyTo.messageId = histories->convertTopicReplyToId(
			history,
			file->to.replyTo.messageId);
		file->to.replyTo.topicRootId = histories->convertTopicReplyToId(
			history,
			file->to.replyTo.topicRootId);
	}

	session->uploader().upload(newId, file);

	auto action = SendAction(history, file->to.options);
	action.clearDraft = false;
	action.replyTo = file->to.replyTo;
	action.generateLocal = true;
	action.replaceMediaOf = file->to.replaceMediaOf;
	session->api().sendAction(action);

	auto caption = TextWithEntities{
		file->caption.text,
		TextUtilities::ConvertTextTagsToEntities(file->caption.tags)
	};
	const auto prepareFlags = Ui::ItemTextOptions(
		history,
		session->user()).flags;
	TextUtilities::PrepareForSending(caption, prepareFlags);
	TextUtilities::Trim(caption);

	auto flags = isEditing ? MessageFlags() : NewMessageFlags(peer);
	if (file->to.replyTo) {
		flags |= MessageFlag::HasReplyInfo;
	}
	FillMessagePostFlags(action, peer, flags);
	if (file->to.options.scheduled) {
		flags |= MessageFlag::IsOrWasScheduled;

		// Scheduled messages have no 'edited' badge.
		flags |= MessageFlag::HideEdited;
	}
	if (file->to.options.shortcutId) {
		flags |= MessageFlag::ShortcutMessage;

		// Shortcut messages have no 'edited' badge.
		flags |= MessageFlag::HideEdited;
	}
	if (file->type == SendMediaType::Audio) {
		if (!peer->isChannel() || peer->isMegagroup()) {
			flags |= MessageFlag::MediaIsUnread;
		}
	}
	if (file->to.options.invertCaption) {
		flags |= MessageFlag::InvertMedia;
	}
	const auto media = MTPMessageMedia([&] {
		if (file->type == SendMediaType::Photo) {
			using Flag = MTPDmessageMediaPhoto::Flag;
			return MTP_messageMediaPhoto(
				MTP_flags(Flag::f_photo
					| (file->spoiler ? Flag::f_spoiler : Flag())),
				file->photo,
				MTPint());
		} else if (file->type == SendMediaType::File) {
			using Flag = MTPDmessageMediaDocument::Flag;
			return MTP_messageMediaDocument(
				MTP_flags(Flag::f_document
					| (file->spoiler ? Flag::f_spoiler : Flag())),
				file->document,
				MTPVector<MTPDocument>(), // alt_documents
				MTPint());
		} else if (file->type == SendMediaType::Audio) {
			const auto ttlSeconds = file->to.options.ttlSeconds;
			const auto isVoice = [&] {
				return file->document.match([](const MTPDdocumentEmpty &d) {
					return false;
				}, [](const MTPDdocument &d) {
					return ranges::any_of(d.vattributes().v, [&](
							const MTPDocumentAttribute &attribute) {
						using Att = MTPDdocumentAttributeAudio;
						return attribute.match([](const Att &data) -> bool {
							return data.vflags().v & Att::Flag::f_voice;
						}, [](const auto &) {
							return false;
						});
					});
				});
			}();
			using Flag = MTPDmessageMediaDocument::Flag;
			return MTP_messageMediaDocument(
				MTP_flags(Flag::f_document
					| (isVoice ? Flag::f_voice : Flag())
					| (ttlSeconds ? Flag::f_ttl_seconds : Flag())),
				file->document,
				MTPVector<MTPDocument>(), // alt_documents
				MTP_int(ttlSeconds));
		} else {
			Unexpected("Type in sendFilesConfirmed.");
		}
	}());

	if (itemToEdit) {
		auto edition = HistoryMessageEdition();
		edition.isEditHide = (flags & MessageFlag::HideEdited);
		edition.editDate = 0;
		edition.ttl = 0;
		edition.mtpMedia = &media;
		edition.textWithEntities = caption;
		edition.invertMedia = file->to.options.invertCaption;
		edition.useSameViews = true;
		edition.useSameForwards = true;
		edition.useSameMarkup = true;
		edition.useSameReplies = true;
		edition.useSameReactions = true;
		edition.savePreviousMedia = true;
		itemToEdit->applyEdition(std::move(edition));
	} else {
		history->addNewLocalMessage({
			.id = newId.msg,
			.flags = flags,
			.from = NewMessageFromId(action),
			.replyTo = file->to.replyTo,
			.date = NewMessageDate(file->to.options),
			.shortcutId = file->to.options.shortcutId,
			.postAuthor = NewMessagePostAuthor(action),
			.groupedId = groupId,
			.effectId = file->to.options.effectId,
		}, caption, media);
	}

	if (isEditing) {
		return;
	}

	session->data().sendHistoryChangeNotifications();
	if (!itemToEdit) {
		session->changes().historyUpdated(
			history,
			(action.options.scheduled
				? Data::HistoryUpdate::Flag::ScheduledSent
				: Data::HistoryUpdate::Flag::MessageSent));
	}
#endif

	const auto ready = [=](Storage::ReadyFileWithThumbnail result) {
		const auto content = MessageContentFromFile(session, file, result);

		if (file->to.replaceMediaOf) {
			session->sender().request(TLeditMessageMedia(
				peerToTdbChat(file->to.peer),
				tl_int53(file->to.replaceMediaOf.bare),
				content
			)).send();
			return;
		}

		const auto history = session->data().history(file->to.peer);
		auto action = Api::SendAction(history);
		action.options = file->to.options;
		action.clearDraft = false;
		action.replyTo = file->to.replyTo;
		action.generateLocal = true;
		session->api().sendAction(action);

		if (const auto album = file->album.get()) {
			const auto i = ranges::find(
				album->items,
				file->taskId,
				&SendingAlbum::Item::taskId);
			Assert(i != album->items.end());

			i->content = std::make_unique<TLinputMessageContent>(content);
			SendPreparedAlbumIfReady(action, album);
		} else {
			SendPreparedMessage(action, content);
		}
	};
	session->uploader().start(file, ready);
}

TLmessageSendOptions MessageSendOptions(
		not_null<PeerData*> peer,
		const SendAction &action) {
	return tl_messageSendOptions(
		tl_bool(ShouldSendSilent(peer, action.options)),
		tl_bool(false), // from_background
		tl_bool(false), // update_order_of_installed_stickers_sets
		ScheduledToTL(action.options.scheduled),
		tl_int64(action.options.effectId),
		tl_int32(sendingId),
		tl_bool(false)); // only_preview
}

std::optional<TLinputMessageReplyTo> MessageReplyTo(
		not_null<History*> history,
		const FullReplyTo &replyTo) {
	if (const auto &storyId = replyTo.storyId) {
		return tl_inputMessageReplyToStory(
			peerToTdbChat(storyId.peer),
			tl_int32(storyId.story));
	} else if (const auto messageId = replyTo.messageId) {
		// Complex logic for external replies.
		// Reply should be external if done to another thread.
		const auto to = LookupReplyTo(history, messageId);
		const auto replyingToTopic = replyTo.topicRootId
			? history->peer->forumTopicFor(replyTo.topicRootId)
			: nullptr;
		const auto replyingToTopicId = replyTo.topicRootId
			? (replyingToTopic
				? replyingToTopic->rootId()
				: Data::ForumTopic::kGeneralId)
			: (to ? to->topicRootId() : Data::ForumTopic::kGeneralId);
		const auto replyToTopicId = to
			? to->topicRootId()
			: replyingToTopicId;
		const auto external = (replyTo.messageId.peer != history->peer->id)
			|| (replyingToTopicId != replyToTopicId);
		if (external) {
			return tl_inputMessageReplyToExternalMessage(
				peerToTdbChat(messageId.peer),
				tl_int53(messageId.msg.bare),
				(replyTo.quote.empty()
					? std::optional<TLinputTextQuote>()
					: tl_inputTextQuote(
						Api::FormattedTextToTdb(replyTo.quote),
						tl_int32(replyTo.quoteOffset))));
		}
		return tl_inputMessageReplyToMessage(
			tl_int53(messageId.msg.bare),
			(replyTo.quote.empty()
				? std::optional<TLinputTextQuote>()
				: tl_inputTextQuote(
					Api::FormattedTextToTdb(replyTo.quote),
					tl_int32(replyTo.quoteOffset))));
	}
	return std::nullopt;
}

std::optional<TLinputMessageReplyTo> MessageReplyTo(
		const SendAction &action) {
	return MessageReplyTo(action.history, action.replyTo);
}

TLint53 MessageThreadId(
		not_null<PeerData*> peer,
		const SendAction &action) {
	return tl_int53(action.replyTo.topicRootId.bare);
}

void SendPreparedMessage(
		const SendAction &action,
		TLinputMessageContent content,
		std::optional<MsgId> localMessageId) {
	const auto history = action.history;
	const auto peer = history->peer;
	const auto topicRootId = action.replyTo ? action.topicRootId : 0;
	const auto clearCloudDraft = (content.type() == id_inputMessageText)
		&& content.c_inputMessageText().vclear_draft().v;
	if (clearCloudDraft) {
		history->clearCloudDraft(topicRootId);
		history->startSavingCloudDraft(topicRootId);
	}
	const auto session = &peer->session();
	session->sender().request(TLsendMessage(
		peerToTdbChat(peer->id),
		tl_int53(topicRootId.bare),
		tl_int53(action.replyTo.bare),
		MessageSendOptions(peer, action),
		std::move(content)
	)).done([=] {
		if (clearCloudDraft) {
			history->finishSavingCloudDraftNow(topicRootId);
		}
	}).fail([=](const Error &error) {
		const auto code = error.code;
		//if (error.type() == qstr("MESSAGE_EMPTY")) {
		//	lastMessage->destroy();
		//} else {
		//	sendMessageFail(error, peer, randomId, newId);
		//}
		if (clearCloudDraft) {
			history->finishSavingCloudDraftNow(topicRootId);
		}
	}).send();
}

std::optional<TLmessageSchedulingState> ScheduledToTL(TimeId scheduled) {
	if (!scheduled) {
		return std::nullopt;
	} else if (scheduled == kScheduledTillOnline) {
		return tl_messageSchedulingStateSendWhenOnline();
	} else {
		return tl_messageSchedulingStateSendAtDate(tl_int32(scheduled));
	}
}

} // namespace Api
