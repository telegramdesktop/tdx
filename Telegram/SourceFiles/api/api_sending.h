/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class History;
class PhotoData;
class DocumentData;
struct FilePrepareResult;

namespace Data {
struct InputVenue;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

namespace Tdb {
class TLinputMessageContent;
class TLmessageSchedulingState;
class TLmessageSendOptions;
class TLmessageReplyTo;
} // namespace Tdb

namespace InlineBots {
class Result;
} // namespace InlineBots

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

// We can't create Data::LocationPoint() and use it
// for a local sending message, because we can't request
// map thumbnail in messages history without access hash.
void SendLocation(SendAction action, float64 lat, float64 lon);

void SendVenue(SendAction action, Data::InputVenue venue);

void FillMessagePostFlags(
	const SendAction &action,
	not_null<PeerData*> peer,
	MessageFlags &flags);

void SendConfirmedFile(
	not_null<Main::Session*> session,
	const std::shared_ptr<FilePrepareResult> &file);

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

void TryGenerateLocalInlineResultMessage(
	not_null<UserData*> bot,
	not_null<InlineBots::Result*> data,
	const SendAction &action,
	MsgId localId);

} // namespace Api
