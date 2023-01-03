/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "history/view/media/history_view_sticker.h"
#include "history/view/media/history_view_service_box.h"

namespace Data {
class MediaGiftBox;
} // namespace Data

namespace HistoryView {

class PremiumGift final : public ServiceBoxContent {
public:
#if 0 // mtp
	PremiumGift(
		not_null<Element*> parent,
		not_null<Data::MediaGiftBox*> gift);
#endif
	PremiumGift(
		not_null<Element*> parent,
		not_null<Data::MediaGiftBox*> gift,
		DocumentData *document);
	~PremiumGift();

	int top() override;
	QSize size() override;
	QString title() override;
	TextWithEntities subtitle() override;
	QString button() override;
	int buttonSkip() override;
	void draw(
		Painter &p,
		const PaintContext &context,
		const QRect &geometry) override;
	ClickHandlerPtr createViewLink() override;

	bool hideServiceText() override {
		return false;
	}

	void stickerClearLoopPlayed() override;
	std::unique_ptr<StickerPlayer> stickerTakePlayer(
		not_null<DocumentData*> data,
		const Lottie::ColorReplacements *replacements) override;

	bool hasHeavyPart() override;
	void unloadHeavyPart() override;

private:
	void ensureStickerCreated() const;

	const not_null<Element*> _parent;
	const not_null<Data::MediaGiftBox*> _gift;
	DocumentData *_document = nullptr;
	mutable std::optional<Sticker> _sticker;

};

} // namespace HistoryView
