/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/controls/title_sub_widget.h"

#include "ui/text/text.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "styles/style_widgets.h"
#include "styles/style_window.h"

namespace Ui {

style::TextStyle MakeTextStyle(QFont font) {
	const auto single = style::font(font.pixelSize(), 0, font.family());
	return {
		.font = single,
		.linkUnderline = st::kLinkUnderlineNever,
	};
}

const style::TextStyle &SmallTitleTextStyle() {
	static const auto result = [] {
		const auto font = style::font(
			style::ConvertScale(10),
			style::FontFlag::Semibold,
			0);
		return style::TextStyle{
			.font = font,
			.linkUnderline = st::kLinkUnderlineNever,
		};
	}();
	return result;
}

not_null<RpWidget*> CreateTitleSubWidget(
		not_null<RpWidget*> parent,
		const style::TextStyle &st,
		rpl::producer<QString> text,
		rpl::producer<style::align> align,
		rpl::producer<bool> shown,
		bool progress) {
	const auto result = CreateChild<RpWidget>(parent.get());
	if (shown) {
		result->showOn(std::move(shown));
	} else {
		result->show();
	}

	struct State {
		const style::TextStyle &st;
		Text::String text;
	};
	const auto state = result->lifetime().make_state<State>(State{
		.st = st,
	});
	rpl::combine(
		std::move(text),
		parent->heightValue()
	) | rpl::start_with_next([=](QString text, int height) {
		state->text.setText(state->st, text);

		const auto added = st::titleSubWidgetLeft.width()
			+ st::titleSubWidgetRight.width();
		result->resize(added + state->text.maxWidth(), height);
	}, result->lifetime());

	rpl::combine(
		parent->sizeValue(),
		result->widthValue(),
		std::move(align)
	) | rpl::start_with_next([=](QSize size, int width, style::align align) {
		const auto skip = (size.height() - state->st.font->height) / 2;
		const auto left = (align & Qt::AlignLeft)
			? (2 * skip - st::titleSubWidgetLeft.width())
			: (align & Qt::AlignRight)
			? (size.width() + st::titleSubWidgetRight.width() - width - 2 * skip)
			: (size.width() - width) / 2;
		result->move(left, 0);
	}, result->lifetime());

	result->paintRequest(
	) | rpl::start_with_next([=](QRect clip) {
		auto p = Painter(result);
		const auto active = result->window()->isActiveWindow();
		const auto &left = st::titleSubWidgetLeft;
		const auto &right = st::titleSubWidgetRight;
		const auto width = result->width();
		const auto height = result->height();
		const auto leftArea = QRect(0, 0, left.width(), height);
		const auto rightArea = QRect(
			width - right.width(),
			0,
			right.width(),
			height);
		const auto middleArea = QRect(
			left.width(),
			0,
			width - left.width() - right.width(),
			height);
		const auto fill = [&](QRect area, const style::icon &icon) {
			if (area.intersects(clip)) {
				if (active) {
					icon.fill(p, area);
				} else {
					icon.fill(p, area, st::titleBg->c);
				}
			}
		};
		fill(leftArea, left);
		fill(rightArea, right);
		if (const auto middle = middleArea.intersected(clip); !middle.isEmpty()) {
			p.fillRect(middle, active ? st::titleBgActive : st::titleBg);
			const auto fontHeight = state->st.font->height;
			p.setPen(active ? st::titleFgActive : st::titleFg);
			state->text.draw(p, left.width(), (height - fontHeight) / 2, width);
		}
	}, result->lifetime());

	return result;
}

} // namespace Ui
