/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/iv_prepare.h"

#include "base/openssl_help.h"
#include "base/unixtime.h"
#include "iv/iv_data.h"
#include "lang/lang_keys.h"
#include "ui/image/image_prepare.h"
#include "ui/grouped_layout.h"
#include "styles/palette.h"
#include "styles/style_chat.h"

#include <QtCore/QSize>

namespace Iv {
namespace {

using namespace Tdb;

struct Attribute {
	QByteArray name;
	std::optional<QByteArray> value;
};
using Attributes = std::vector<Attribute>;

struct Photo {
	uint64 id = 0;
	int width = 0;
	int height = 0;
	QByteArray minithumbnail;
};

struct Document {
	uint64 id = 0;
	int width = 0;
	int height = 0;
	QByteArray minithumbnail;
};

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
[[nodiscard]] QByteArray Number(T value) {
	return QByteArray::number(value);
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
[[nodiscard]] QByteArray Percent(T value) {
	return Number(base::SafeRound(value * 10000.) / 100.);
};

[[nodiscard]] QByteArray Escape(QByteArray value) {
	auto result = QByteArray();
	result.reserve(value.size());
	for (const auto &ch : value) {
		switch (ch) {
		case '&': result.append("&amp;"); break;
		case '<': result.append("&lt;"); break;
		case '>': result.append("&gt;"); break;
		case '"': result.append("&quot;"); break;
		case '\'': result.append("&apos;"); break;
		default: result.append(ch); break;
		}
	}
	return result;
}

[[nodiscard]] QByteArray Escape(const QString &utf16) {
	return Escape(utf16.toUtf8());
}

[[nodiscard]] QByteArray Date(TimeId date) {
	return Escape(langDateTimeFull(base::unixtime::parse(date)).toUtf8());
}

class Parser final {
public:
	Parser(const Source &source, const Options &options);

	[[nodiscard]] Prepared result();

private:
#if 0 // mtp
	void process(const Source &source);
	void process(const MTPPhoto &photo);
	void process(const MTPDocument &document);

	template <typename Inner>
	[[nodiscard]] QByteArray list(const MTPVector<Inner> &data);

	[[nodiscard]] QByteArray collage(
		const QVector<MTPPageBlock> &list,
		const std::vector<QSize> &dimensions,
		int offset = 0);
	[[nodiscard]] QByteArray slideshow(
		const QVector<MTPPageBlock> &list,
		QSize dimensions);

	[[nodiscard]] QByteArray block(const MTPDpageBlockUnsupported &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockTitle &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockSubtitle &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockAuthorDate &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockHeader &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockSubheader &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockParagraph &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockPreformatted &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockFooter &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockDivider &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockAnchor &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockList &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockBlockquote &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockPullquote &data);
	[[nodiscard]] QByteArray block(
		const MTPDpageBlockPhoto &data,
		const Ui::GroupMediaLayout &layout = {},
		QSize outer = {});
	[[nodiscard]] QByteArray block(
		const MTPDpageBlockVideo &data,
		const Ui::GroupMediaLayout &layout = {},
		QSize outer = {});
	[[nodiscard]] QByteArray block(const MTPDpageBlockCover &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockEmbed &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockEmbedPost &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockCollage &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockSlideshow &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockChannel &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockAudio &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockKicker &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockTable &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockOrderedList &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockDetails &data);
	[[nodiscard]] QByteArray block(
		const MTPDpageBlockRelatedArticles &data);
	[[nodiscard]] QByteArray block(const MTPDpageBlockMap &data);

	[[nodiscard]] QByteArray block(const MTPDpageRelatedArticle &data);

	[[nodiscard]] QByteArray block(const MTPDpageTableRow &data);
	[[nodiscard]] QByteArray block(const MTPDpageTableCell &data);

	[[nodiscard]] QByteArray block(const MTPDpageListItemText &data);
	[[nodiscard]] QByteArray block(const MTPDpageListItemBlocks &data);

	[[nodiscard]] QByteArray block(const MTPDpageListOrderedItemText &data);
	[[nodiscard]] QByteArray block(
		const MTPDpageListOrderedItemBlocks &data);
#endif
	[[nodiscard]] Photo process(const TLphoto *photo);
	[[nodiscard]] Document process(const TLdocument *document);
	[[nodiscard]] Document process(const TLvideo *video);
	[[nodiscard]] Document process(const TLanimation *animation);
	[[nodiscard]] Document process(const TLsticker *sticker);
	[[nodiscard]] Document process(const TLaudio *audio);
	[[nodiscard]] Document process(const TLvoiceNote *note);

	template <typename Inner>
	[[nodiscard]] QByteArray list(const TLvector<TLvector<Inner>> &data);
	template <typename Inner>
	[[nodiscard]] QByteArray list(const TLvector<Inner> &data);

	[[nodiscard]] QByteArray collage(
		const QVector<TLpageBlock> &list,
		const std::vector<QSize> &dimensions,
		int offset = 0);
	[[nodiscard]] QByteArray slideshow(
		const QVector<TLpageBlock> &list,
		QSize dimensions);

	//[[nodiscard]] QByteArray block(const MTPDpageBlockUnsupported &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockTitle &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockSubtitle &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockAuthorDate &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockHeader &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockSubheader &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockParagraph &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockPreformatted &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockFooter &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockDivider &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockAnchor &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockList &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockBlockQuote &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockPullQuote &data);
	[[nodiscard]] QByteArray block(
		const TLDpageBlockPhoto &data,
		const Ui::GroupMediaLayout &layout = {},
		QSize outer = {});
	[[nodiscard]] QByteArray block(
		const TLDpageBlockVideo &data,
		const Ui::GroupMediaLayout &layout = {},
		QSize outer = {});
	[[nodiscard]] QByteArray block(
		const TLDpageBlockAnimation &data,
		const Ui::GroupMediaLayout &layout = {},
		QSize outer = {});
	[[nodiscard]] QByteArray block(const TLDpageBlockCover &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockEmbedded &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockEmbeddedPost &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockCollage &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockSlideshow &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockChatLink &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockAudio &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockVoiceNote &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockKicker &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockTable &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockDetails &data);
	[[nodiscard]] QByteArray block(
		const TLDpageBlockRelatedArticles &data);
	[[nodiscard]] QByteArray block(const TLDpageBlockMap &data);

	[[nodiscard]] QByteArray block(const TLDpageBlockRelatedArticle &data);

	[[nodiscard]] QByteArray block(const TLDpageBlockTableCell &data);

	[[nodiscard]] QByteArray block(const TLDpageBlockListItem &data);

	[[nodiscard]] QByteArray block(const TLpageBlockTableCell &data);
	[[nodiscard]] QByteArray block(
		const TLvector<TLpageBlockTableCell> &data);
	[[nodiscard]] QByteArray block(const TLpageBlockListItem &data);

	[[nodiscard]] QByteArray wrap(const QByteArray &content, int views);

	[[nodiscard]] QByteArray tag(
		const QByteArray &name,
		const QByteArray &body = {});
	[[nodiscard]] QByteArray tag(
		const QByteArray &name,
		const Attributes &attributes,
		const QByteArray &body = {});
#if 0 // mtp
	[[nodiscard]] QByteArray utf(const MTPstring &text);
	[[nodiscard]] QByteArray utf(const tl::conditional<MTPstring> &text);
	[[nodiscard]] QByteArray rich(const MTPRichText &text);
	[[nodiscard]] QByteArray caption(const MTPPageCaption &caption);

	[[nodiscard]] Photo parse(const MTPPhoto &photo);
	[[nodiscard]] Document parse(const MTPDocument &document);
	[[nodiscard]] Geo parse(const MTPGeoPoint &geo);
#endif
	[[nodiscard]] QByteArray utf(const TLstring &text);
	[[nodiscard]] QByteArray rich(const TLrichText &text);
	[[nodiscard]] QByteArray caption(const TLpageBlockCaption &caption);

	[[nodiscard]] Photo parse(const TLphoto &photo);
	[[nodiscard]] Document parse(const TLdocument &document);
	[[nodiscard]] Document parse(const TLvideo &video);
	[[nodiscard]] Document parse(const TLanimation &animation);
	[[nodiscard]] Document parse(const TLsticker &sticker);
	[[nodiscard]] Document parse(const TLaudio &audio);
	[[nodiscard]] Document parse(const TLvoiceNote &note);
	[[nodiscard]] Geo parse(const TLlocation &location);

	[[nodiscard]] Photo photoById(uint64 id);
	[[nodiscard]] Document documentById(uint64 id);

	[[nodiscard]] QByteArray photoFullUrl(const Photo &photo);
	[[nodiscard]] QByteArray documentFullUrl(const Document &document);
	[[nodiscard]] QByteArray embedUrl(const QByteArray &html);
	[[nodiscard]] QByteArray mapUrl(
		const Geo &geo,
		int width,
		int height,
		int zoom);
	[[nodiscard]] QByteArray resource(QByteArray id);

#if 0 // mtp
	[[nodiscard]] std::vector<QSize> computeCollageDimensions(
		const QVector<MTPPageBlock> &items);
	[[nodiscard]] QSize computeSlideshowDimensions(
		const QVector<MTPPageBlock> &items);
#endif
	[[nodiscard]] std::vector<QSize> computeCollageDimensions(
		const QVector<TLpageBlock> &items);
	[[nodiscard]] QSize computeSlideshowDimensions(
		const QVector<TLpageBlock> &items);

	//const Options _options;
	const QByteArray _fileOriginPostfix;

	base::flat_set<QByteArray> _resources;

	Prepared _result;

	base::flat_map<uint64, Photo> _photosById;
	base::flat_map<uint64, Document> _documentsById;

};

[[nodiscard]] bool IsVoidElement(const QByteArray &name) {
	// Thanks https://developer.mozilla.org/en-US/docs/Glossary/Void_element
	static const auto voids = base::flat_set<QByteArray>{
		"area"_q,
		"base"_q,
		"br"_q,
		"col"_q,
		"embed"_q,
		"hr"_q,
		"img"_q,
		"input"_q,
		"link"_q,
		"meta"_q,
		"source"_q,
		"track"_q,
		"wbr"_q,
	};
	return voids.contains(name);
}

[[nodiscard]] QByteArray ArrowSvg(bool left) {
	const auto rotate = QByteArray(left ? "180" : "0");
	return R"(
<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
	<path
		d="M14.9972363,18 L9.13865768,12.1414214 C9.06055283,12.0633165 9.06055283,11.9366835 9.13865768,11.8585786 L14.9972363,6 L14.9972363,6"
		transform="translate(11.997236, 12) scale(-1, -1) rotate()" + rotate + ") translate(-11.997236, -12)" + R"(">
	</path>
</svg>)";
}

Parser::Parser(const Source &source, const Options &options)
: /*_options(options)
, */_fileOriginPostfix('/' + Number(source.pageId)) {
#if 0 // mtp
	process(source);
#endif
	_result.pageId = source.pageId;
	_result.name = source.name;
#if 0 // mtp
	_result.rtl = source.page.data().is_rtl();

	const auto views = std::max(
		source.page.data().vviews().value_or_empty(),
		source.updatedCachedViews);
	const auto content = list(source.page.data().vblocks());
#endif
	_result.rtl = source.page.data().vis_rtl().v;
	const auto views = std::max(
		source.page.data().vview_count().v,
		source.updatedCachedViews);
	const auto content = list(source.page.data().vpage_blocks());
	_result.content = wrap(content, views);
}

Prepared Parser::result() {
	return _result;
}

#if 0 // mtp
void Parser::process(const Source &source) {
	const auto &data = source.page.data();
	for (const auto &photo : data.vphotos().v) {
		process(photo);
	}
	for (const auto &document : data.vdocuments().v) {
		process(document);
	}
	if (source.webpagePhoto) {
		process(*source.webpagePhoto);
	}
	if (source.webpageDocument) {
		process(*source.webpageDocument);
	}
}

void Parser::process(const MTPPhoto &photo) {
	_photosById.emplace(
		photo.match([](const auto &data) { return data.vid().v; }),
		parse(photo));
}

void Parser::process(const MTPDocument &document) {
	_documentsById.emplace(
		document.match([](const auto &data) { return data.vid().v; }),
		parse(document));
}
#endif

Photo Parser::process(const TLphoto *photo) {
	if (!photo) {
		return {};
	}
	const auto &sizes = photo->data().vsizes().v;
	const auto id = sizes.isEmpty()
		? 0
		: sizes.front().data().vphoto().data().vid().v;
	return _photosById.emplace(id, parse(*photo)).first->second;
}

Document Parser::process(const TLdocument *document) {
	if (!document) {
		return {};
	}
	return _documentsById.emplace(
		document->data().vdocument().data().vid().v,
		parse(*document)).first->second;
}

Document Parser::process(const TLvideo *video) {
	if (!video) {
		return {};
	}
	return _documentsById.emplace(
		video->data().vvideo().data().vid().v,
		parse(*video)).first->second;
}

Document Parser::process(const TLanimation *animation) {
	if (!animation) {
		return {};
	}
	return _documentsById.emplace(
		animation->data().vanimation().data().vid().v,
		parse(*animation)).first->second;
}

Document Parser::process(const TLsticker *sticker) {
	if (!sticker) {
		return {};
	}
	return _documentsById.emplace(
		sticker->data().vsticker().data().vid().v,
		parse(*sticker)).first->second;
}

Document Parser::process(const TLaudio *audio) {
	if (!audio) {
		return {};
	}
	return _documentsById.emplace(
		audio->data().vaudio().data().vid().v,
		parse(*audio)).first->second;
}

Document Parser::process(const TLvoiceNote *note) {
	if (!note) {
		return {};
	}
	return _documentsById.emplace(
		note->data().vvoice().data().vid().v,
		parse(*note)).first->second;
}

template <typename Inner>
QByteArray Parser::list(const TLvector<TLvector<Inner>> &data) {
	auto result = QByteArrayList();
	result.reserve(data.v.size());
	for (const auto &item : data.v) {
		result.append(list(item));
	}
	return result.join(QByteArray());
}

template <typename Inner>
#if 0 // mtp
QByteArray Parser::list(const MTPVector<Inner> &data) {
#endif
QByteArray Parser::list(const TLvector<Inner> &data) {
	auto result = QByteArrayList();
	result.reserve(data.v.size());
	for (const auto &item : data.v) {
		result.append(item.match([&](const auto &data) {
			return block(data);
		}));
	}
	return result.join(QByteArray());
}

#if 0 // mtp
QByteArray Parser::collage(
		const QVector<MTPPageBlock> &list,
#endif
QByteArray Parser::collage(
		const QVector<TLpageBlock> &list,
		const std::vector<QSize> &dimensions,
		int offset) {
	Expects(list.size() == dimensions.size());

	constexpr auto kPerCollage = 10;
	const auto last = (offset + kPerCollage >= int(dimensions.size()));

	auto result = QByteArray();
	auto slice = ((offset > 0) || (dimensions.size() > kPerCollage))
		? (dimensions
			| ranges::views::drop(offset)
			| ranges::views::take(kPerCollage)
			| ranges::to_vector)
		: dimensions;
	const auto layout = Ui::LayoutMediaGroup(
		slice,
		st::historyGroupWidthMax,
		st::historyGroupWidthMin,
		st::historyGroupSkip);
	auto size = QSize();
	for (const auto &part : layout) {
		const auto &rect = part.geometry;
		size = QSize(
			std::max(size.width(), rect.x() + rect.width()),
			std::max(size.height(), rect.y() + rect.height()));
	}
	for (auto i = 0, count = int(layout.size()); i != count; ++i) {
		const auto &part = layout[i];
#if 0 // mtp
		list[offset + i].match([&](const MTPDpageBlockPhoto &data) {
#endif
		list[offset + i].match([&](const TLDpageBlockPhoto &data) {
			result += block(data, part, size);
#if 0 // mtp
		}, [&](const MTPDpageBlockVideo &data) {
#endif
		}, [&](const TLDpageBlockVideo &data) {
			result += block(data, part, size);
		}, [](const auto &) {
			Unexpected("Block type in collage layout.");
		});
	}
	const auto aspectHeight = size.height() / float64(size.width());
	const auto aspectSkip = st::historyGroupSkip / float64(size.width());
	auto wrapped = tag("figure", {
		{ "class", "collage" },
		{
			"style",
			("padding-top: " + Percent(aspectHeight) + "%; "
				+ "margin-bottom: " + Percent(last ? 0 : aspectSkip) + "%;")
		},
	}, result);
	if (offset + kPerCollage < int(dimensions.size())) {
		wrapped += collage(list, dimensions, offset + kPerCollage);
	}
	return wrapped;
}

#if 0 // mtp
QByteArray Parser::slideshow(
		const QVector<MTPPageBlock> &list,
#endif
QByteArray Parser::slideshow(
		const QVector<TLpageBlock> &list,
		QSize dimensions) {
	auto result = QByteArray();
	for (auto i = 0, count = int(list.size()); i != count; ++i) {
#if 0 // mtp
		list[i].match([&](const MTPDpageBlockPhoto &data) {
#endif
		list[i].match([&](const TLDpageBlockPhoto &data) {
			result += block(data, {}, dimensions);
#if 0 // mtp
		}, [&](const MTPDpageBlockVideo &data) {
#endif
		}, [&](const TLDpageBlockVideo &data) {
			result += block(data, {}, dimensions);
		}, [](const auto &) {
			Unexpected("Block type in collage layout.");
		});
	}

	auto inputs = QByteArrayList();
	for (auto i = 0; i != int(list.size()); ++i) {
		auto attributes = Attributes{
			{ "type", "radio" },
			{ "name", "s" },
			{ "value", Number(i) },
			{ "onchange", "return IV.slideshowSlide(this);" },
		};
		if (!i) {
			attributes.push_back({ "checked", std::nullopt });
		}
		inputs.append(tag("label", tag("input", attributes, tag("i"))));
	}
	const auto form = tag(
		"form",
		{ { "class", "slideshow-buttons" } },
		tag("fieldset", inputs.join(QByteArray())));
	const auto navigation = tag("a", {
		{ "class", "slideshow-prev" },
		{ "onclick", "IV.slideshowSlide(this, -1);" },
	}, ArrowSvg(true)) + tag("a", {
		{ "class", "slideshow-next" },
		{ "onclick", "IV.slideshowSlide(this, 1);" },
	}, ArrowSvg(false));
	auto wrapStyle = "padding-top: calc(min("
		+ Percent(dimensions.height() / float64(dimensions.width()))
		+ "%, 480px));";
	result = form + tag("figure", {
		{ "class", "slideshow" },
	}, result) + navigation;
	return tag("figure", {
		{ "class", "slideshow-wrap" },
		{ "style", wrapStyle },
	}, result);
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockUnsupported &data) {
	return QByteArray();
}

QByteArray Parser::block(const MTPDpageBlockTitle &data) {
	return tag("h1", { { "class", "title" } }, rich(data.vtext()));
#endif
QByteArray Parser::block(const TLDpageBlockTitle &data) {
	return tag("h1", { { "class", "title" } }, rich(data.vtitle()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockSubtitle &data) {
	return tag("h2", { { "class", "subtitle" } }, rich(data.vtext()));
#endif
QByteArray Parser::block(const TLDpageBlockSubtitle & data) {
	return tag("h2", { { "class", "subtitle" } }, rich(data.vsubtitle()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockAuthorDate &data) {
#endif
QByteArray Parser::block(const TLDpageBlockAuthorDate &data) {
	auto inner = rich(data.vauthor());
#if 0 // mtp
	if (const auto date = data.vpublished_date().v) {
#endif
	if (const auto date = data.vpublish_date().v) {
		inner += " \xE2\x80\xA2 " + tag("time", Date(date));
	}
	return tag("address", inner);
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockHeader &data) {
	return tag("h3", { { "class", "header" } }, rich(data.vtext()));
#endif
QByteArray Parser::block(const TLDpageBlockHeader &data) {
	return tag("h3", { { "class", "header" } }, rich(data.vheader()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockSubheader &data) {
	return tag("h4", { { "class", "subheader" } }, rich(data.vtext()));
#endif
QByteArray Parser::block(const TLDpageBlockSubheader &data) {
	return tag("h4", { { "class", "subheader" } }, rich(data.vsubheader()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockParagraph &data) {
#endif
QByteArray Parser::block(const TLDpageBlockParagraph &data) {
	return tag("p", rich(data.vtext()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockPreformatted &data) {
#endif
QByteArray Parser::block(const TLDpageBlockPreformatted &data) {
	auto list = Attributes();
	const auto language = utf(data.vlanguage());
	if (!language.isEmpty()) {
		list.push_back({ "data-language", language });
		list.push_back({ "class", "lang-" + language });
		_result.hasCode = true;
	}
	return tag("pre", list, rich(data.vtext()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockFooter &data) {
	return tag("footer", { { "class", "footer" } }, rich(data.vtext()));
#endif
QByteArray Parser::block(const TLDpageBlockFooter &data) {
	return tag("footer", { { "class", "footer" } }, rich(data.vfooter()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockDivider &data) {
#endif
QByteArray Parser::block(const TLDpageBlockDivider &data) {
	return tag("hr", Attributes{ { "class", "divider" } });
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockAnchor &data) {
#endif
QByteArray Parser::block(const TLDpageBlockAnchor &data) {
	return tag("a", { { "name", utf(data.vname()) } });
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockList &data) {
#endif
QByteArray Parser::block(const TLDpageBlockList &data) {
	return tag("ul", list(data.vitems()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockBlockquote &data) {
	const auto caption = rich(data.vcaption());
#endif
QByteArray Parser::block(const TLDpageBlockBlockQuote &data) {
	const auto caption = rich(data.vcredit());
	const auto cite = caption.isEmpty()
		? QByteArray()
		: tag("cite", caption);
	return tag("blockquote", rich(data.vtext()) + cite);
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockPullquote &data) {
	const auto caption = rich(data.vcaption());
#endif
QByteArray Parser::block(const TLDpageBlockPullQuote &data) {
	const auto caption = rich(data.vcredit());
	const auto cite = caption.isEmpty()
		? QByteArray()
		: tag("cite", caption);
	return tag(
		"div",
		{ { "class", "pullquote" } },
		rich(data.vtext()) + cite);
}

#if 0 // mtp
QByteArray Parser::block(
		const MTPDpageBlockPhoto &data,
#endif
QByteArray Parser::block(
		const TLDpageBlockPhoto &data,
		const Ui::GroupMediaLayout &layout,
		QSize outer) {
	const auto collage = !layout.geometry.isEmpty();
	const auto slideshow = !collage && !outer.isEmpty();
#if 0 // mtp
	const auto photo = photoById(data.vphoto_id().v);
#endif
	const auto photo = process(data.vphoto());
	if (!photo.id) {
		return "Photo not found.";
	}
	const auto src = photoFullUrl(photo);
	auto wrapStyle = QByteArray();
	if (collage) {
		const auto wcoef = 1. / outer.width();
		const auto hcoef = 1. / outer.height();
		wrapStyle += "left: " + Percent(layout.geometry.x() * wcoef) + "%; "
			+ "top: " + Percent(layout.geometry.y() * hcoef) + "%; "
			+ "width: " + Percent(layout.geometry.width() * wcoef) + "%; "
			+ "height: " + Percent(layout.geometry.height() * hcoef) + "%";
	} else if (!slideshow && photo.width) {
		wrapStyle += "max-width:" + Number(photo.width) + "px";
	}
	const auto dimension = collage
		? (layout.geometry.height() / float64(layout.geometry.width()))
		: (photo.width && photo.height)
		? (photo.height / float64(photo.width))
		: (3 / 4.);
	const auto paddingTop = collage
		? Percent(dimension) + "%"
		: "calc(min(480px, " + Percent(dimension) + "%))";
	const auto style = "background-image:url('" + src + "');"
		"padding-top: " + paddingTop + ";";
	auto inner = tag("div", {
		{ "class", "photo" },
		{ "style", style } });
	const auto minithumb = Images::ExpandInlineBytes(photo.minithumbnail);
	if (!minithumb.isEmpty()) {
		const auto image = Images::Read({ .content = minithumb });
		inner = tag("div", {
			{ "class", "photo-bg" },
			{ "style", "background-image:url('data:image/jpeg;base64,"
				+ minithumb.toBase64()
				+ "');" },
		}) + inner;
	}
	auto attributes = Attributes{
		{ "class", "photo-wrap" },
		{ "style", wrapStyle }
	};
	auto result = tag("div", attributes, inner);

#if 0 // mtp
	const auto href = data.vurl() ? utf(*data.vurl()) : photoFullUrl(photo);
#endif
	const auto emptyUrl = data.vurl().v.isEmpty();
	const auto href = emptyUrl ? photoFullUrl(photo) : utf(data.vurl());
	const auto id = Number(photo.id);
	result = tag("a", {
		{ "href", href },
#if 0 // mtp
		{ "oncontextmenu", data.vurl() ? QByteArray() : "return false;" },
		{ "data-context", data.vurl() ? QByteArray() : "viewer-photo" + id },
#endif
		{ "oncontextmenu", emptyUrl ? "return false;" : QByteArray() },
		{ "data-context", emptyUrl ? "viewer-photo" + id : QByteArray() },
	}, result);
	if (!slideshow) {
		result += caption(data.vcaption());
		if (!collage) {
			result = tag("div", { { "class", "media-outer" } }, result);
		}
	}
	return result;
}

#if 0 // mtp
QByteArray Parser::block(
		const MTPDpageBlockVideo &data,
#endif
QByteArray Parser::block(
		const TLDpageBlockVideo &data,
		const Ui::GroupMediaLayout &layout,
		QSize outer) {
	const auto collage = !layout.geometry.isEmpty();
	const auto slideshow = !collage && !outer.isEmpty();
	const auto collageSmall = collage
		&& (layout.geometry.width() < outer.width());
#if 0 // mtp
	const auto video = documentById(data.vvideo_id().v);
#endif
	const auto video = process(data.vvideo());
	if (!video.id) {
		return "Video not found.";
	}
	auto inner = tag("div", {
		{ "class", "video" },
		{ "data-src", documentFullUrl(video) },
#if 0 // mtp
		{ "data-autoplay", data.is_autoplay() ? "1" : "0" },
		{ "data-loop", data.is_loop() ? "1" : "0" },
#endif
		{ "data-autoplay", data.vneed_autoplay().v ? "1" : "0" },
		{ "data-loop", data.vis_looped().v ? "1" : "0" },
		{ "data-small", collageSmall ? "1" : "0" },
	});
	const auto minithumb = Images::ExpandInlineBytes(video.minithumbnail);
	if (!minithumb.isEmpty()) {
		const auto image = Images::Read({ .content = minithumb });
		inner = tag("div", {
			{ "class", "video-bg" },
			{ "style", "background-image:url('data:image/jpeg;base64,"
				+ minithumb.toBase64()
				+ "');" },
		}) + inner;
	}
	auto wrapStyle = QByteArray();
	if (collage) {
		const auto wcoef = 1. / outer.width();
		const auto hcoef = 1. / outer.height();
		wrapStyle += "left: " + Percent(layout.geometry.x() * wcoef) + "%; "
			+ "top: " + Percent(layout.geometry.y() * hcoef) + "%; "
			+ "width: " + Percent(layout.geometry.width() * wcoef) + "%; "
			+ "height: " + Percent(layout.geometry.height() * hcoef) + "%; ";
	} else {
		const auto dimension = (video.width && video.height)
			? (video.height / float64(video.width))
			: (3 / 4.);
		wrapStyle += "padding-top: calc(min(480px, "
			+ Percent(dimension)
			+ "%));";
	}
	auto attributes = Attributes{
		{ "class", "video-wrap" },
		{ "style", wrapStyle },
	};
	auto result = tag("div", attributes, inner);
#if 0 // mtp
	if (data.is_autoplay() || collageSmall) {
#endif
	if (data.vneed_autoplay().v || collageSmall) {
		const auto id = Number(video.id);
		const auto href = resource("video" + id);
		result = tag("a", {
			{ "href", href },
			{ "oncontextmenu", "return false;" },
			{ "data-context", "viewer-video" + id },
		}, result);
	}
	if (!slideshow) {
		result += caption(data.vcaption());
		if (!collage) {
			result = tag("div", { { "class", "media-outer" } }, result);
		}
	}
	return result;
}

QByteArray Parser::block(
		const TLDpageBlockAnimation &data,
		const Ui::GroupMediaLayout &layout,
		QSize outer) {
	const auto collage = !layout.geometry.isEmpty();
	const auto slideshow = !collage && !outer.isEmpty();
	const auto collageSmall = collage
		&& (layout.geometry.width() < outer.width());
	const auto animation = process(data.vanimation());
	if (!animation.id) {
		return "Animation not found.";
	}
	auto inner = tag("div", {
		{ "class", "video" },
		{ "data-src", documentFullUrl(animation) },
		{ "data-autoplay", data.vneed_autoplay().v ? "1" : "0" },
		{ "data-loop", "1" },
		{ "data-small", collageSmall ? "1" : "0" },
	});
	const auto minithumb = Images::ExpandInlineBytes(
		animation.minithumbnail);
	if (!minithumb.isEmpty()) {
		const auto image = Images::Read({ .content = minithumb });
		inner = tag("div", {
			{ "class", "video-bg" },
			{ "style", "background-image:url('data:image/jpeg;base64,"
				+ minithumb.toBase64()
				+ "');" },
		}) + inner;
	}
	auto wrapStyle = QByteArray();
	if (collage) {
		const auto wcoef = 1. / outer.width();
		const auto hcoef = 1. / outer.height();
		wrapStyle += "left: " + Percent(layout.geometry.x() * wcoef) + "%; "
			+ "top: " + Percent(layout.geometry.y() * hcoef) + "%; "
			+ "width: " + Percent(layout.geometry.width() * wcoef) + "%; "
			+ "height: " + Percent(layout.geometry.height() * hcoef) + "%; ";
	} else {
		const auto dimension = (animation.width && animation.height)
			? (animation.height / float64(animation.width))
			: (3 / 4.);
		wrapStyle += "padding-top: calc(min(480px, "
			+ Percent(dimension)
			+ "%));";
	}
	auto attributes = Attributes{
		{ "class", "video-wrap" },
		{ "style", wrapStyle },
	};
	auto result = tag("div", attributes, inner);
	if (data.vneed_autoplay().v || collageSmall) {
		const auto id = Number(animation.id);
		const auto href = resource("video" + id);
		result = tag("a", {
			{ "href", href },
			{ "oncontextmenu", "return false;" },
			{ "data-context", "viewer-video" + id },
		}, result);
	}
	if (!slideshow) {
		result += caption(data.vcaption());
		if (!collage) {
			result = tag("div", { { "class", "media-outer" } }, result);
		}
	}
	return result;
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockCover &data) {
#endif
QByteArray Parser::block(const TLDpageBlockCover &data) {
	return tag("figure", data.vcover().match([&](const auto &data) {
		return block(data);
	}));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockEmbed &data) {
	_result.hasEmbeds = true;
	auto eclass = data.is_full_width() ? QByteArray() : "nowide";
#endif
QByteArray Parser::block(const TLDpageBlockEmbedded &data) {
	_result.hasEmbeds = true;
	auto eclass = data.vis_full_width().v ? QByteArray() : "nowide";
	auto width = QByteArray();
	auto height = QByteArray();
	auto iframeWidth = QByteArray();
	auto iframeHeight = QByteArray();
#if 0 // mtp
	const auto autosize = !data.vw();
	if (autosize) {
		iframeWidth = "100%";
		eclass = "nowide";
	} else if (data.is_full_width() || !data.vw()->v) {
		width = "100%";
		height = Number(data.vh()->v) + "px";
		iframeWidth = "100%";
		iframeHeight = height;
	} else {
		width = Number(data.vw()->v) + "px";
		height = Percent(data.vh()->v / float64(data.vw()->v)) + "%";
	}
#endif
	const auto autosize = !data.vwidth().v;
	if (autosize) {
		iframeWidth = "100%";
		eclass = "nowide";
	} else if (data.vis_full_width().v || !data.vwidth().v) {
		width = "100%";
		height = Number(data.vheight().v) + "px";
		iframeWidth = "100%";
		iframeHeight = height;
	} else {
		width = Number(data.vwidth().v) + "px";
		height = Percent(data.vheight().v / float64(data.vwidth().v)) + "%";
	}
	auto attributes = Attributes();
	if (autosize) {
		attributes.push_back({ "class", "autosize" });
	}
	attributes.push_back({ "width", iframeWidth });
	attributes.push_back({ "height", iframeHeight });
#if 0 // mtp
	if (const auto url = data.vurl()) {
#endif
	if (const auto url = data.vurl(); !url.v.isEmpty()) {
		if (!autosize) {
			attributes.push_back({ "src", utf(url) });
		} else {
			attributes.push_back({ "srcdoc", utf(url) });
		}
#if 0 // mtp
	} else if (const auto html = data.vhtml()) {
		attributes.push_back({ "src", embedUrl(html->v) });
	}
	if (!data.is_allow_scrolling()) {
#endif
	} else if (const auto html = data.vhtml(); !html.v.isEmpty()) {
		attributes.push_back({ "src", embedUrl(html.v.toUtf8()) });
	}
	if (!data.vallow_scrolling().v) {
		attributes.push_back({ "scrolling", "no" });
	}
	attributes.push_back({ "frameborder", "0" });
	attributes.push_back({ "allowtransparency", "true" });
	attributes.push_back({ "allowfullscreen", "true" });
	auto result = tag("iframe", attributes);
	if (!autosize) {
		result = tag("div", {
			{ "class", "iframe-wrap" },
			{ "style", "width:" + width },
		}, tag("div", {
			{ "style", "padding-bottom: " + height },
		}, result));
	}
	result += caption(data.vcaption());
	return tag("figure", { { "class", eclass } }, result);
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockEmbedPost &data) {
	auto result = QByteArray();
	if (!data.vblocks().v.isEmpty()) {
		auto address = QByteArray();
		const auto photo = photoById(data.vauthor_photo_id().v);
#endif
QByteArray Parser::block(const TLDpageBlockEmbeddedPost &data) {
	auto result = QByteArray();
	if (!data.vpage_blocks().v.isEmpty()) {
		auto address = QByteArray();
		const auto photo = process(data.vauthor_photo());
		if (photo.id) {
			const auto src = photoFullUrl(photo);
			address += tag(
				"figure",
				{ { "style", "background-image:url('" + src + "')" } });
		}
		address += tag(
			"a",
			{ { "rel", "author" }, { "onclick", "return false;" } },
			utf(data.vauthor()));
		if (const auto date = data.vdate().v) {
			const auto parsed = base::unixtime::parse(date);
			address += tag("time", Date(date));
		}
#if 0 // mtp
		const auto inner = tag("address", address) + list(data.vblocks());
#endif
		const auto inner = tag("address", address)
			+ list(data.vpage_blocks());
		result = tag("blockquote", { { "class", "embed-post" } }, inner);
	} else {
		const auto url = utf(data.vurl());
		const auto inner = tag("strong", utf(data.vauthor()))
			+ tag(
				"small",
				tag("a", { { "href", url } }, url));
		result = tag("section", { { "class", "embed-post" } }, inner);
	}
	result += caption(data.vcaption());
	return tag("figure", result);
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockCollage &data) {
	const auto &items = data.vitems().v;
#endif
QByteArray Parser::block(const TLDpageBlockCollage &data) {
	const auto &items = data.vpage_blocks().v;
	const auto dimensions = computeCollageDimensions(items);
	if (dimensions.empty()) {
		return tag(
			"figure",
#if 0 // mtp
			tag("figure", list(data.vitems())) + caption(data.vcaption()));
#endif
			tag("figure", list(data.vpage_blocks()))
				+ caption(data.vcaption()));
	}

	return tag(
		"figure",
		{ { "class", "collage-wrap" } },
		collage(items, dimensions) + caption(data.vcaption()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockSlideshow &data) {
	const auto &items = data.vitems().v;
#endif
QByteArray Parser::block(const TLDpageBlockSlideshow &data) {
	const auto &items = data.vpage_blocks().v;
	const auto dimensions = computeSlideshowDimensions(items);
	if (dimensions.isEmpty()) {
#if 0 // mtp
		return list(data.vitems());
#endif
		return list(data.vpage_blocks());
	}
	const auto result = slideshow(items, dimensions);
	return tag("figure", result + caption(data.vcaption()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockChannel &data) {
	auto name = QByteArray();
	auto username = QByteArray();
	auto id = data.vchannel().match([](const auto &data) {
		return Number(data.vid().v);
	});
	data.vchannel().match([&](const MTPDchannel &data) {
		if (const auto has = data.vusername()) {
			username = utf(*has);
		}
		name = utf(data.vtitle());
	}, [&](const MTPDchat &data) {
		name = utf(data.vtitle());
	}, [](const auto &) {
	});
#endif
QByteArray Parser::block(const TLDpageBlockChatLink &data) {
	auto name = utf(data.vtitle());
	auto username = utf(data.vusername());
	auto id = username.toBase64();
	auto result = tag(
		"div",
		{ { "class", "join" }, { "data-context", "join_link" + id } },
		tag("span")
	) + tag("h4", name);
	const auto link = username.isEmpty()
		? "javascript:alert('Channel Link');"
		: "https://t.me/" + username;
	result = tag(
		"a",
		{ { "href", link }, { "data-context", "channel" + id } },
		result);
	_result.channelIds.emplace(id);
	return tag("section", {
		{ "class", "channel joined" },
		{ "data-context", "channel" + id },
	}, result);
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockAudio &data) {
	const auto audio = documentById(data.vaudio_id().v);
#endif
QByteArray Parser::block(const TLDpageBlockAudio &data) {
	const auto audio = process(data.vaudio());
	if (!audio.id) {
		return "Audio not found.";
	}
	const auto src = documentFullUrl(audio);
	return tag("figure", tag("audio", {
		{ "src", src },
		{ "oncontextmenu", "return false;" },
		{ "controls", std::nullopt },
	}) + caption(data.vcaption()));
}

QByteArray Parser::block(const TLDpageBlockVoiceNote &data) {
	const auto voice = process(data.vvoice_note());
	if (!voice.id) {
		return "Voice not found.";
	}
	const auto src = documentFullUrl(voice);
	return tag("figure", tag("audio", {
		{ "src", src },
		{ "oncontextmenu", "return false;" },
		{ "controls", std::nullopt },
	}) + caption(data.vcaption()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockKicker &data) {
	return tag("h5", { { "class", "kicker" } }, rich(data.vtext()));
#endif
QByteArray Parser::block(const TLDpageBlockKicker &data) {
	return tag("h5", { { "class", "kicker" } }, rich(data.vkicker()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockTable &data) {
	auto classes = QByteArrayList();
	if (data.is_bordered()) {
		classes.push_back("bordered");
	}
	if (data.is_striped()) {
#endif
QByteArray Parser::block(const TLDpageBlockTable &data) {
	auto classes = QByteArrayList();
	if (data.vis_bordered().v) {
		classes.push_back("bordered");
	}
	if (data.vis_striped().v) {
		classes.push_back("striped");
	}
	auto attibutes = Attributes();
	if (!classes.isEmpty()) {
		attibutes.push_back({ "class", classes.join(" ") });
	}
#if 0 // mtp
	auto title = rich(data.vtitle());
#endif
	auto title = rich(data.vcaption());
	if (!title.isEmpty()) {
		title = tag("caption", title);
	}
#if 0 // mtp
	auto result = tag("table", attibutes, title + list(data.vrows()));
#endif
	auto result = tag("table", attibutes, title + list(data.vcells()));
	result = tag("figure", { { "class", "table" } }, result);
	result = tag("figure", { { "class", "table-wrap" } }, result);
	return tag("figure", result);
}

QByteArray Parser::block(const TLDpageBlockListItem &data) {
	return list(data.vpage_blocks());
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockOrderedList &data) {
	return tag("ol", list(data.vitems()));
}

QByteArray Parser::block(const MTPDpageBlockDetails &data) {
#endif
QByteArray Parser::block(const TLDpageBlockDetails &data) {
	auto attributes = Attributes();
#if 0 // mtp
	if (data.is_open()) {
#endif
	if (data.vis_open().v) {
		attributes.push_back({ "open", std::nullopt });
	}
	return tag(
		"details",
		attributes,
#if 0 // mtp
		tag("summary", rich(data.vtitle())) + list(data.vblocks()));
#endif
	tag("summary", rich(data.vheader())) + list(data.vpage_blocks()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockRelatedArticles &data) {
#endif
QByteArray Parser::block(const TLDpageBlockRelatedArticles &data) {
	const auto result = list(data.varticles());
	if (result.isEmpty()) {
		return QByteArray();
	}
#if 0 // mtp
	auto title = rich(data.vtitle());
#endif
	auto title = rich(data.vheader());
	if (!title.isEmpty()) {
		title = tag("h4", { { "class", "related-title" } }, title);
	}
	return tag("section", { { "class", "related" } }, title + result);
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageBlockMap &data) {
	const auto geo = parse(data.vgeo());
#endif
QByteArray Parser::block(const TLDpageBlockMap &data) {
	const auto geo = parse(data.vlocation());
	if (!geo.access) {
		return "Map not found.";
	}
	const auto width = 650;
#if 0 // mtp
	const auto height = std::min(450, (data.vh().v * width / data.vw().v));
#endif
	const auto height = std::min(
		450,
		(data.vheight().v * width / data.vwidth().v));
	return tag("figure", tag("img", {
		{ "src", mapUrl(geo, width, height, data.vzoom().v) },
	}) + caption(data.vcaption()));
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageRelatedArticle &data) {
#endif
QByteArray Parser::block(const TLDpageBlockRelatedArticle &data) {
	auto result = QByteArray();
#if 0 // mtp
	const auto photo = photoById(data.vphoto_id().value_or_empty());
#endif
	const auto photo = process(data.vphoto());
	if (photo.id) {
		const auto src = photoFullUrl(photo);
		result += tag("i", {
			{ "class", "related-link-thumb" },
			{ "style", "background-image:url('" + src + "')" },
		});
	}
	const auto title = data.vtitle();
	const auto description = data.vdescription();
	const auto author = data.vauthor();
#if 0 // mtp
	const auto published = data.vpublished_date();
	if (title || description || author || published) {
		auto inner = QByteArray();
		if (title) {
			inner += tag(
				"span",
				{ { "class", "related-link-title" } },
				utf(*title));
		}
		if (description) {
			inner += tag(
				"span",
				{ { "class", "related-link-desc" } },
				utf(*description));
		}
		if (author || published) {
			inner += tag(
				"span",
				{ { "class", "related-link-source" } },
				((author ? utf(*author) : QByteArray())
					+ ((author && published) ? ", " : QByteArray())
					+ (published ? Date(published->v) : QByteArray())));
		}
#endif
	const auto published = data.vpublish_date().v;
	if (!title.v.isEmpty() || !description.v.isEmpty() || !author.v.isEmpty() || published) {
		auto inner = QByteArray();
		if (!title.v.isEmpty()) {
			inner += tag(
				"span",
				{ { "class", "related-link-title" } },
				utf(title));
		}
		if (!description.v.isEmpty()) {
			inner += tag(
				"span",
				{ { "class", "related-link-desc" } },
				utf(description));
		}
		if (!author.v.isEmpty() || published) {
			inner += tag(
				"span",
				{ { "class", "related-link-source" } },
				((author.v.isEmpty() ? QByteArray() : utf(author))
					+ ((!author.v.isEmpty() && published) ? ", " : QByteArray())
					+ (published ? Date(published) : QByteArray())));
		}
		result += tag("span", {
			{ "class", "related-link-content" },
		}, inner);
	}
#if 0 // mtp
	const auto webpageId = data.vwebpage_id().v;
#endif
	const auto webpageId = 0;
	const auto context = webpageId
		? ("webpage" + Number(webpageId))
		: QByteArray();
	return tag("a", {
		{ "class", "related-link" },
		{ "href", utf(data.vurl()) },
		{ "data-context", context },
	}, result);
}

QByteArray Parser::block(const TLpageBlockTableCell &data) {
	return block(data.data());
}

QByteArray Parser::block(
		const TLvector<TLpageBlockTableCell> &data) {
	return list(data);
}

QByteArray Parser::block(const TLpageBlockListItem &data) {
	return block(data.data());
}

#if 0 // mtp
QByteArray Parser::block(const MTPDpageTableRow &data) {
	return tag("tr", list(data.vcells()));
}

QByteArray Parser::block(const MTPDpageTableCell &data) {
#endif
QByteArray Parser::block(const TLDpageBlockTableCell &data) {
	const auto text = data.vtext() ? rich(*data.vtext()) : QByteArray();
	auto style = QByteArray();
	data.valign().match([&](const TLDpageBlockHorizontalAlignmentRight &) {
		style += "text-align:right;";
	}, [&](const TLDpageBlockHorizontalAlignmentCenter &) {
		style += "text-align:center;";
	}, [&](const TLDpageBlockHorizontalAlignmentLeft &) {
		style += "text-align:left;";
	});
	data.vvalign().match([&](const TLDpageBlockVerticalAlignmentBottom &) {
		style += "vertical-align:bottom;";
	}, [&](const TLDpageBlockVerticalAlignmentMiddle &) {
		style += "vertical-align:middle;";
	}, [&](const TLDpageBlockVerticalAlignmentTop &) {
		style += "vertical-align:top;";
	});
#if 0 // mtp
	if (data.is_align_right()) {
		style += "text-align:right;";
	} else if (data.is_align_center()) {
		style += "text-align:center;";
	} else {
		style += "text-align:left;";
	}
	if (data.is_valign_bottom()) {
		style += "vertical-align:bottom;";
	} else if (data.is_valign_middle()) {
		style += "vertical-align:middle;";
	} else {
		style += "vertical-align:top;";
	}
#endif
	auto attributes = Attributes{ { "style", style } };
#if 0 // mtp
	if (const auto cs = data.vcolspan()) {
		attributes.push_back({ "colspan", Number(cs->v) });
	}
	if (const auto rs = data.vrowspan()) {
		attributes.push_back({ "rowspan", Number(rs->v) });
	}
#endif
	if (const auto cs = data.vcolspan().v) {
		attributes.push_back({ "colspan", Number(cs) });
	}
	if (const auto rs = data.vrowspan().v) {
		attributes.push_back({ "rowspan", Number(rs) });
	}
	return tag(data.vis_header().v ? "th" : "td", attributes, text);
}
#if 0 // mtp
	return tag(data.is_header() ? "th" : "td", attributes, text);
}

QByteArray Parser::block(const MTPDpageListItemText &data) {
	return tag("li", rich(data.vtext()));
}

QByteArray Parser::block(const MTPDpageListItemBlocks &data) {
	return tag("li", list(data.vblocks()));
}

QByteArray Parser::block(const MTPDpageListOrderedItemText &data) {
	return tag(
		"li",
		{ { "value", utf(data.vnum()) } },
		rich(data.vtext()));
}

QByteArray Parser::block(const MTPDpageListOrderedItemBlocks &data) {
	return tag(
		"li",
		{ { "value", utf(data.vnum()) } },
		list(data.vblocks()));
}

QByteArray Parser::utf(const MTPstring &text) {
#endif
QByteArray Parser::utf(const TLstring &text) {
	return Escape(text.v);
}

#if 0 // mtp
QByteArray Parser::utf(const tl::conditional<MTPstring> &text) {
	return text ? utf(*text) : QByteArray();
}
#endif

QByteArray Parser::wrap(const QByteArray &content, int views) {
	const auto sep = " \xE2\x80\xA2 ";
	const auto viewsText = views
		? (tr::lng_stories_views(tr::now, lt_count_decimal, views) + sep)
		: QString();
	return R"(
<div class="page-slide">
	<article>)"_q + content + R"(</article>
</div>
<div class="page-footer">
	<div class="content">
		)"_q
		+ viewsText.toUtf8()
		+ R"(<a class="wrong" data-context="report-iv">)"_q
		+ tr::lng_iv_wrong_layout(tr::now).toUtf8()
		+ R"(</a>
	</div>
</div>)"_q;
}

QByteArray Parser::tag(
		const QByteArray &name,
		const QByteArray &body) {
	return tag(name, {}, body);
}

QByteArray Parser::tag(
		const QByteArray &name,
		const Attributes &attributes,
		const QByteArray &body) {
	auto list = QByteArrayList();
	list.reserve(attributes.size());
	for (auto &[name, value] : attributes) {
		list.push_back(' ' + name + (value ? "=\"" + *value + "\"" : ""));
	}
	const auto serialized = list.join(QByteArray());
	return (IsVoidElement(name) && body.isEmpty())
		? ('<' + name + serialized + " />")
		: ('<' + name + serialized + '>' + body + "</" + name + '>');
}

#if 0 // mtp
QByteArray Parser::rich(const MTPRichText &text) {
	return text.match([&](const MTPDtextEmpty &data) {
		return QByteArray();
	}, [&](const MTPDtextPlain &data) {
#endif
QByteArray Parser::rich(const TLrichText &text) {
	return text.match([&](const TLDrichTextPlain &data) {
		struct Replacement {
			QByteArray from;
			QByteArray to;
		};
		const auto replacements = std::vector<Replacement>{
			{ "\xE2\x81\xA6", "<span dir=\"ltr\">" },
			{ "\xE2\x81\xA7", "<span dir=\"rtl\">" },
			{ "\xE2\x81\xA8", "<span dir=\"auto\">" },
			{ "\xE2\x81\xA9", "</span>" },
		};
		auto text = utf(data.vtext());
		for (const auto &[from, to] : replacements) {
			text.replace(from, to);
		}
		return text;
#if 0 // mtp
	}, [&](const MTPDtextConcat &data) {
#endif
	}, [&](const TLDrichTexts &data) {
		const auto &list = data.vtexts().v;
		auto result = QByteArrayList();
		result.reserve(list.size());
		for (const auto &item : list) {
			result.append(rich(item));
		}
		return result.join(QByteArray());
#if 0 // mtp
	}, [&](const MTPDtextImage &data) {
		const auto image = documentById(data.vdocument_id().v);
#endif
	}, [&](const TLDrichTextIcon &data) {
		const auto image = process(&data.vdocument());
		if (!image.id) {
			return "Image not found."_q;
		}
		auto attributes = Attributes{
			{ "class", "pic" },
			{ "src", documentFullUrl(image) },
		};
#if 0 // mtp
		if (const auto width = data.vw().v) {
#endif
		if (const auto width = data.vwidth().v) {
			attributes.push_back({ "width", Number(width) });
		}
#if 0 // mtp
		if (const auto height = data.vh().v) {
#endif
		if (const auto height = data.vheight().v) {
			attributes.push_back({ "height", Number(height) });
		}
		return tag("img", attributes);
#if 0 // mtp
	}, [&](const MTPDtextBold &data) {
#endif
	}, [&](const TLDrichTextBold &data) {
		return tag("b", rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextItalic &data) {
#endif
	}, [&](const TLDrichTextItalic &data) {
		return tag("i", rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextUnderline &data) {
#endif
	}, [&](const TLDrichTextUnderline &data) {
		return tag("u", rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextStrike &data) {
#endif
	}, [&](const TLDrichTextStrikethrough &data) {
		return tag("s", rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextFixed &data) {
#endif
	}, [&](const TLDrichTextFixed &data) {
		return tag("code", rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextUrl &data) {
		const auto webpageId = data.vwebpage_id().v;
#endif
	}, [&](const TLDrichTextUrl &data) {
		const auto webpageId = data.vis_cached().v ? 1 : 0;
		const auto context = webpageId
			? ("webpage" + Number(webpageId))
			: QByteArray();
		return tag("a", {
			{ "href", utf(data.vurl()) },
			{ "class", webpageId ? "internal-iv-link" : "" },
			{ "data-context", context },
		}, rich(data.vtext()));
	}, [&](const TLDrichTextReference &data) {
		const auto context = QByteArray();
		return tag("a", {
			{ "href", utf(data.vurl())},
			{ "class", "" },
			{ "data-context", context },
		}, rich(data.vtext()));
	}, [&](const TLDrichTextAnchorLink &data) {
		const auto context = QByteArray();
		return tag("a", {
			{ "href", utf(data.vurl())},
			{ "class", "" },
			{ "data-context", context },
		}, rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextEmail &data) {
		return tag("a", {
			{ "href", "mailto:" + utf(data.vemail()) },
#endif
	}, [&](const TLDrichTextEmailAddress &data) {
		return tag("a", {
			{ "href", "mailto:" + utf(data.vemail_address()) },
		}, rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextSubscript &data) {
#endif
	}, [&](const TLDrichTextSubscript &data) {
		return tag("sub", rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextSuperscript &data) {
#endif
	}, [&](const TLDrichTextSuperscript &data) {
		return tag("sup", rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextMarked &data) {
#endif
	}, [&](const TLDrichTextMarked &data) {
		return tag("mark", rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextPhone &data) {
		return tag("a", {
			{ "href", "tel:" + utf(data.vphone()) },
#endif
	}, [&](const TLDrichTextPhoneNumber &data) {
		return tag("a", {
			{ "href", "tel:" + utf(data.vphone_number()) },
		}, rich(data.vtext()));
#if 0 // mtp
	}, [&](const MTPDtextAnchor &data) {
		const auto inner = rich(data.vtext());
#endif
	}, [&](const TLDrichTextAnchor &data) {
		const auto inner = QByteArray();
		const auto name = utf(data.vname());
		return inner.isEmpty()
			? tag("a", { { "name", name } })
			: tag(
				"span",
				{ { "class", "reference" } },
				tag("a", { { "name", name } }) + inner);
	});
}

#if 0 // mtp
QByteArray Parser::caption(const MTPPageCaption &caption) {
#endif
QByteArray Parser::caption(const TLpageBlockCaption &caption) {
	auto text = rich(caption.data().vtext());
	const auto credit = rich(caption.data().vcredit());
	if (!credit.isEmpty()) {
		text += tag("cite", credit);
	} else if (text.isEmpty()) {
		return QByteArray();
	}
	return tag("figcaption", text);
}

#if 0 // mtp
Photo Parser::parse(const MTPPhoto &photo) {
	auto result = Photo{
		.id = photo.match([&](const auto &d) { return d.vid().v; }),
	};
	auto sizes = base::flat_map<QByteArray, QSize>();
	photo.match([](const MTPDphotoEmpty &) {
	}, [&](const MTPDphoto &data) {
		for (const auto &size : data.vsizes().v) {
			size.match([&](const MTPDphotoSizeEmpty &data) {
			}, [&](const MTPDphotoSize &data) {
				sizes.emplace(
					data.vtype().v,
					QSize(data.vw().v, data.vh().v));
			}, [&](const MTPDphotoCachedSize &data) {
				sizes.emplace(
					data.vtype().v,
					QSize(data.vw().v, data.vh().v));
			}, [&](const MTPDphotoStrippedSize &data) {
				result.minithumbnail = data.vbytes().v;
			}, [&](const MTPDphotoSizeProgressive &data) {
				sizes.emplace(
					data.vtype().v,
					QSize(data.vw().v, data.vh().v));
			}, [&](const MTPDphotoPathSize &data) {
			});
		}
	});
	for (const auto attempt : { "y", "x", "w" }) {
		const auto i = sizes.find(QByteArray(attempt));
		if (i != end(sizes)) {
			result.width = i->second.width();
			result.height = i->second.height();
			break;
		}
	}
	return result;
}

Document Parser::parse(const MTPDocument &document) {
	auto result = Document{
		.id = document.match([&](const auto &d) { return d.vid().v; }),
	};
	document.match([](const MTPDdocumentEmpty &) {
	}, [&](const MTPDdocument &data) {
		for (const auto &attribute : data.vattributes().v) {
			attribute.match([&](const MTPDdocumentAttributeImageSize &data) {
				result.width = data.vw().v;
				result.height = data.vh().v;
			}, [&](const MTPDdocumentAttributeVideo &data) {
				result.width = data.vw().v;
				result.height = data.vh().v;
			}, [](const auto &) {});
		}
		if (const auto sizes = data.vthumbs()) {
			for (const auto &size : sizes->v) {
				size.match([&](const MTPDphotoStrippedSize &data) {
					result.minithumbnail = data.vbytes().v;
				}, [&](const auto &data) {
				});
			}

		}
	});
	return result;
}

Geo Parser::parse(const MTPGeoPoint &geo) {
	return geo.match([](const MTPDgeoPointEmpty &) {
		return Geo();
	}, [&](const MTPDgeoPoint &data) {
		return Geo{
			.lat = data.vlat().v,
			.lon = data.vlong().v,
			.access = data.vaccess_hash().v,
		};
	});
}
#endif
Photo Parser::parse(const TLphoto &photo) {
	const auto &sizes = photo.data().vsizes().v;
	const auto id = sizes.isEmpty()
		? 0
		: sizes.front().data().vphoto().data().vid().v;
	auto result = Photo{
		.id = uint64(id),
	};
	auto list = base::flat_map<QByteArray, QSize>();
	if (const auto mini = photo.data().vminithumbnail()) {
		result.minithumbnail = mini->data().vdata().v;
	}
	for (const auto &size : sizes) {
		const auto &data = size.data();
		list.emplace(
			utf(data.vtype()),
			QSize(data.vwidth().v, data.vheight().v));
	}
	for (const auto attempt : { "y", "x", "w" }) {
		const auto i = list.find(QByteArray(attempt));
		if (i != end(list)) {
			result.width = i->second.width();
			result.height = i->second.height();
			break;
		}
	}
	return result;
}

Document Parser::parse(const TLdocument &document) {
	auto result = Document{
		.id = uint64(document.data().vdocument().data().vid().v),
	};
	if (const auto mini = document.data().vminithumbnail()) {
		result.minithumbnail = mini->data().vdata().v;
	}
	return result;
}

Document Parser::parse(const TLvideo &video) {
	auto result = Document{
		.id = uint64(video.data().vvideo().data().vid().v),
	};
	if (const auto mini = video.data().vminithumbnail()) {
		result.minithumbnail = mini->data().vdata().v;
	}
	result.width = video.data().vwidth().v;
	result.height = video.data().vheight().v;
	return result;
}

Document Parser::parse(const TLanimation &animation) {
	auto result = Document{
		.id = uint64(animation.data().vanimation().data().vid().v),
	};
	if (const auto mini = animation.data().vminithumbnail()) {
		result.minithumbnail = mini->data().vdata().v;
	}
	result.width = animation.data().vwidth().v;
	result.height = animation.data().vheight().v;
	return result;
}

Document Parser::parse(const TLsticker &sticker) {
	auto result = Document{
		.id = uint64(sticker.data().vsticker().data().vid().v),
	};
	result.width = sticker.data().vwidth().v;
	result.height = sticker.data().vheight().v;
	return result;
}

Document Parser::parse(const TLaudio &audio) {
	auto result = Document{
		.id = uint64(audio.data().vaudio().data().vid().v),
	};
	return result;
}

Document Parser::parse(const TLvoiceNote &note) {
	auto result = Document{
		.id = uint64(note.data().vvoice().data().vid().v),
	};
	return result;
}

Geo Parser::parse(const TLlocation &location) {
	return Geo{
		.lat = location.data().vlatitude().v,
		.lon = location.data().vlongitude().v,
		.access = 1,
	};
}

Photo Parser::photoById(uint64 id) {
	const auto i = _photosById.find(id);
	return (i != end(_photosById)) ? i->second : Photo();
}

Document Parser::documentById(uint64 id) {
	const auto i = _documentsById.find(id);
	return (i != end(_documentsById)) ? i->second : Document();
}

QByteArray Parser::photoFullUrl(const Photo &photo) {
	return resource("photo/" + Number(photo.id) + _fileOriginPostfix);
}

QByteArray Parser::documentFullUrl(const Document &document) {
	return resource("document/" + Number(document.id) + _fileOriginPostfix);
}

QByteArray Parser::embedUrl(const QByteArray &html) {
	auto binary = std::array<uchar, SHA256_DIGEST_LENGTH>{};
	SHA256(
		reinterpret_cast<const unsigned char*>(html.data()),
		html.size(),
		binary.data());
	const auto hex = [](uchar value) -> char {
		return (value >= 10) ? ('a' + (value - 10)) : ('0' + value);
	};
	auto result = QByteArray();
	result.reserve(binary.size() * 2);
	for (const auto byte : binary) {
		result.push_back(hex(byte / 16));
		result.push_back(hex(byte % 16));
	}
	result += ".html";
	_result.embeds.emplace(result, html);
	return resource("html/" + result);
}

QByteArray Parser::mapUrl(const Geo &geo, int width, int height, int zoom) {
	return resource("map/"
		+ GeoPointId(geo) + "&"
		+ Number(width) + ","
		+ Number(height) + "&"
		+ Number(zoom));
}

QByteArray Parser::resource(QByteArray id) {
	return '/' + id;
}

std::vector<QSize> Parser::computeCollageDimensions(
#if 0 // mtp
		const QVector<MTPPageBlock> &items) {
	if (items.size() < 2) {
		return {};
	}
	auto result = std::vector<QSize>(items.size());
	for (auto i = 0, count = int(items.size()); i != count; ++i) {
		items[i].match([&](const MTPDpageBlockPhoto &data) {
			const auto photo = photoById(data.vphoto_id().v);
			if (photo.id && photo.width > 0 && photo.height > 0) {
				result[i] = QSize(photo.width, photo.height);
			}
		}, [&](const MTPDpageBlockVideo &data) {
			const auto document = documentById(data.vvideo_id().v);
#endif
		const QVector<TLpageBlock> &items) {
	if (items.size() < 2) {
		return {};
	}
	auto result = std::vector<QSize>(items.size());
	for (auto i = 0, count = int(items.size()); i != count; ++i) {
		items[i].match([&](const TLDpageBlockPhoto &data) {
			const auto photo = process(data.vphoto());
			if (photo.id && photo.width > 0 && photo.height > 0) {
				result[i] = QSize(photo.width, photo.height);
			}
		}, [&](const TLDpageBlockVideo &data) {
			const auto document = process(data.vvideo());
			if (document.id && document.width > 0 && document.height > 0) {
				result[i] = QSize(document.width, document.height);
			}
		}, [&](const TLDpageBlockAnimation &data) {
			const auto document = process(data.vanimation());
			if (document.id && document.width > 0 && document.height > 0) {
				result[i] = QSize(document.width, document.height);
			}
		}, [](const auto &) {});
		if (result[i].isEmpty()) {
			return {};
		}
	}
	return result;
}

QSize Parser::computeSlideshowDimensions(
#if 0 // mtp
		const QVector<MTPPageBlock> &items) {
#endif
		const QVector<TLpageBlock> &items) {
	if (items.size() < 2) {
		return {};
	}
	auto result = QSize();
	for (const auto &item : items) {
		auto size = QSize();
#if 0 // mtp
		item.match([&](const MTPDpageBlockPhoto &data) {
			const auto photo = photoById(data.vphoto_id().v);
			if (photo.id && photo.width > 0 && photo.height > 0) {
				size = QSize(photo.width, photo.height);
			}
		}, [&](const MTPDpageBlockVideo &data) {
			const auto document = documentById(data.vvideo_id().v);
#endif
		item.match([&](const TLDpageBlockPhoto &data) {
			const auto photo = process(data.vphoto());
			if (photo.id && photo.width > 0 && photo.height > 0) {
				size = QSize(photo.width, photo.height);
			}
		}, [&](const TLDpageBlockVideo &data) {
			const auto document = process(data.vvideo());
			if (document.id && document.width > 0 && document.height > 0) {
				size = QSize(document.width, document.height);
			}
		}, [&](const TLDpageBlockAnimation &data) {
			const auto document = process(data.vanimation());
			if (document.id && document.width > 0 && document.height > 0) {
				size = QSize(document.width, document.height);
			}
		}, [](const auto &) {});
		if (size.isEmpty()) {
			return {};
		} else if (result.height() * size.width()
			< result.width() * size.height()) {
			result = size;
		}
	}
	return result;
}

} // namespace

Prepared Prepare(const Source &source, const Options &options) {
	auto parser = Parser(source, options);
	return parser.result();
}

} // namespace Iv
