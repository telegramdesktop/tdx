/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/sender.h"

#include "tdb/tdb_request_id.h"

class ApiWrap;
class DocumentData;
class PhotoData;

namespace Window {
class SessionController;
} // namespace Window

namespace Api {

class AttachedStickers final {
public:
	explicit AttachedStickers(not_null<ApiWrap*> api);

	void requestAttachedStickerSets(
		not_null<Window::SessionController*> controller,
		not_null<PhotoData*> photo);

	void requestAttachedStickerSets(
		not_null<Window::SessionController*> controller,
		not_null<DocumentData*> document);

private:
#if 0 // mtp
	void request(
		not_null<Window::SessionController*> controller,
		MTPmessages_GetAttachedStickers &&mtpRequest);

	MTP::Sender _api;
	mtpRequestId _requestId = 0;
#endif

	void request(
		not_null<Window::SessionController*> controller,
		FileId fileId);

	const not_null<ApiWrap*> _api;
	Tdb::RequestId _requestId;

};

} // namespace Api
