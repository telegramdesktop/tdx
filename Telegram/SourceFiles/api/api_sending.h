/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Main {
class Session;
} // namespace Main

class History;
class PhotoData;
class DocumentData;
struct FileLoadResult;

namespace Tdb {
class TLinputMessageContent;
class TLmessageSchedulingState;
class TLmessageSendOptions;
class TLmessageReplyTo;
} // namespace Tdb

namespace Api {

struct MessageToSend;
struct SendAction;

void SendExistingDocument(
	MessageToSend &&message,
	not_null<DocumentData*> document,
	std::optional<MsgId> localMessageId = std::nullopt);

void SendExistingPhoto(
	MessageToSend &&message,
	not_null<PhotoData*> photo,
	std::optional<MsgId> localMessageId = std::nullopt);

bool SendDice(MessageToSend &message);

void FillMessagePostFlags(
	const SendAction &action,
	not_null<PeerData*> peer,
	MessageFlags &flags);

void SendConfirmedFile(
	not_null<Main::Session*> session,
	const std::shared_ptr<FileLoadResult> &file);

[[nodiscard]] Tdb::TLmessageSendOptions MessageSendOptions(
	not_null<PeerData*> peer,
	const SendAction &action,
	int32 sendingId = 0);
[[nodiscard]] std::optional<Tdb::TLmessageReplyTo> MessageReplyTo(
	const SendAction &action);

void SendPreparedMessage(
	const SendAction &action,
	Tdb::TLinputMessageContent content,
	std::optional<MsgId> localMessageId = std::nullopt);

[[nodiscard]] std::optional<Tdb::TLmessageSchedulingState> ScheduledToTL(
	TimeId scheduled);

} // namespace Api
