/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_attached_stickers.h"

#include "apiwrap.h"
#include "ui/boxes/confirm_box.h"
#include "boxes/sticker_set_box.h"
#include "boxes/stickers_box.h"
#include "data/data_document.h"
#include "data/data_photo.h"
#include "lang/lang_keys.h"
#include "window/window_session_controller.h"

#include "tdb/tdb_tl_scheme.h"

namespace Api {

using namespace Tdb;

AttachedStickers::AttachedStickers(not_null<ApiWrap*> api)
: _api(&api->instance()) {
}

#if 0 // mtp
void AttachedStickers::request(
		not_null<Window::SessionController*> controller,
		MTPmessages_GetAttachedStickers &&mtpRequest) {
	const auto weak = base::make_weak(controller);
	_api.request(_requestId).cancel();
	_requestId = _api.request(
		std::move(mtpRequest)
	).done([=](const MTPVector<MTPStickerSetCovered> &result) {
		_requestId = 0;
		const auto strongController = weak.get();
		if (!strongController) {
			return;
		}
		if (result.v.isEmpty()) {
			strongController->show(
				Ui::MakeInformBox(tr::lng_stickers_not_found()));
			return;
		} else if (result.v.size() > 1) {
			strongController->show(
				Box<StickersBox>(strongController->uiShow(), result.v));
			return;
		}
		// Single attached sticker pack.
		const auto data = result.v.front().match([&](const auto &data) {
			return &data.vset().data();
		});

		const auto setId = (data->vid().v && data->vaccess_hash().v)
			? StickerSetIdentifier{
				.id = data->vid().v,
				.accessHash = data->vaccess_hash().v }
			: StickerSetIdentifier{ .shortName = qs(data->vshort_name()) };
		strongController->show(Box<StickerSetBox>(
			strongController->uiShow(),
			setId,
			(data->is_emojis()
				? Data::StickersType::Emoji
				: data->is_masks()
				? Data::StickersType::Masks
				: Data::StickersType::Stickers)));
	}).fail([=] {
		_requestId = 0;
		if (const auto strongController = weak.get()) {
			strongController->show(
				Ui::MakeInformBox(tr::lng_stickers_not_found()));
		}
	}).send();
}
#endif

void AttachedStickers::request(
		not_null<Window::SessionController*> controller,
		FileId fileId) {
	const auto weak = base::make_weak(controller.get());
	_api.request(_requestId).cancel();
	_requestId = _api.request(TLgetAttachedStickerSets(
		tl_int32(fileId)
	)).done([=](const TLstickerSets &result) {
		_requestId = 0;
		const auto strongController = weak.get();
		if (!strongController) {
			return;
		}
		const auto &data = result.data();
		const auto &list = data.vsets().v;
		if (list.isEmpty()) {
			strongController->show(
				Ui::MakeInformBox(tr::lng_stickers_not_found()));
			return;
		} else if (list.size() > 1) {
			strongController->show(
				Box<StickersBox>(strongController, data.vsets()));
			return;
		}
		// Single attached sticker pack.
		const auto setId = StickerSetIdentifier{
			.id = uint64(list.front().data().vid().v),
		};
		strongController->show(
			Box<StickerSetBox>(strongController, setId),
			Ui::LayerOption::KeepOther);
	}).fail([=] {
		_requestId = 0;
		if (const auto strongController = weak.get()) {
			strongController->show(
				Ui::MakeInformBox(tr::lng_stickers_not_found()));
		}
	}).send();
}

void AttachedStickers::requestAttachedStickerSets(
		not_null<Window::SessionController*> controller,
		not_null<PhotoData*> photo) {
#if 0 // mtp
	request(
		controller,
		MTPmessages_GetAttachedStickers(
			MTP_inputStickeredMediaPhoto(photo->mtpInput())));
#endif
	const auto &location = photo->location(Data::PhotoSize::Large).file();
	if (const auto tdb = std::get_if<TdbFileLocation>(&location.data)) {
		request(controller, tdb->fileId);
	}
}

void AttachedStickers::requestAttachedStickerSets(
		not_null<Window::SessionController*> controller,
		not_null<DocumentData*> document) {
#if 0 // mtp
	request(
		controller,
		MTPmessages_GetAttachedStickers(
			MTP_inputStickeredMediaDocument(document->mtpInput())));
#endif
	if (const auto fileId = document->tdbFileId()) {
		request(controller, fileId);
	}
}

} // namespace Api
