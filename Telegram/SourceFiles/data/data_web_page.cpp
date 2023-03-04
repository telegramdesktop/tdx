/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_web_page.h"

#include "main/main_session.h"
#include "apiwrap.h"
#include "mainwidget.h"
#include "data/data_session.h"
#include "data/data_photo.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "lang/lang_keys.h"
#include "iv/iv_data.h"
#include "ui/image/image.h"
#include "ui/text/text_entity.h"
#include "tdb/tdb_tl_scheme.h"
#include "api/api_text_entities.h"

#include "data/stickers/data_stickers.h"

#include <xxhash.h>

namespace {

using namespace Tdb;

#if 0 // mtp
[[nodiscard]] WebPageCollage ExtractCollage(
		not_null<Data::Session*> owner,
		const QVector<MTPPageBlock> &items,
		const QVector<MTPPhoto> &photos,
		const QVector<MTPDocument> &documents) {
	const auto count = items.size();
	if (count < 2) {
		return {};
	}
	const auto bad = ranges::find_if(items, [](mtpTypeId type) {
		return (type != mtpc_pageBlockPhoto && type != mtpc_pageBlockVideo);
	}, [](const MTPPageBlock &item) {
		return item.type();
	});
	if (bad != items.end()) {
		return {};
	}

	for (const auto &photo : photos) {
		owner->processPhoto(photo);
	}
	for (const auto &document : documents) {
		owner->processDocument(document);
	}
	auto result = WebPageCollage();
	result.items.reserve(count);
	for (const auto &item : items) {
		const auto good = item.match([&](const MTPDpageBlockPhoto &data) {
			const auto photo = owner->photo(data.vphoto_id().v);
			if (photo->isNull()) {
				return false;
			}
			result.items.emplace_back(photo);
			return true;
		}, [&](const MTPDpageBlockVideo &data) {
			const auto document = owner->document(data.vvideo_id().v);
			if (!document->isVideoFile()) {
				return false;
			}
			result.items.emplace_back(document);
			return true;
		}, [](const auto &) -> bool {
			Unexpected("Type of block in Collage.");
		});
		if (!good) {
			return {};
		}
	}
	return result;
}

WebPageCollage ExtractCollage(
		not_null<Data::Session*> owner,
		const MTPDwebPage &data) {
	const auto page = data.vcached_page();
	if (!page) {
		return {};
	}
	const auto processMedia = [&] {
		if (const auto photo = data.vphoto()) {
			owner->processPhoto(*photo);
		}
		if (const auto document = data.vdocument()) {
			owner->processDocument(*document);
		}
	};
	return page->match([&](const auto &page) {
		for (const auto &block : page.vblocks().v) {
			switch (block.type()) {
			case mtpc_pageBlockPhoto:
			case mtpc_pageBlockVideo:
			case mtpc_pageBlockCover:
			case mtpc_pageBlockEmbed:
			case mtpc_pageBlockEmbedPost:
			case mtpc_pageBlockAudio:
				return WebPageCollage();
			case mtpc_pageBlockSlideshow:
				processMedia();
				return ExtractCollage(
					owner,
					block.c_pageBlockSlideshow().vitems().v,
					page.vphotos().v,
					page.vdocuments().v);
			case mtpc_pageBlockCollage:
				processMedia();
				return ExtractCollage(
					owner,
					block.c_pageBlockCollage().vitems().v,
					page.vphotos().v,
					page.vdocuments().v);
			default: break;
			}
		}
		return WebPageCollage();
	});
}
#endif

WebPageCollage ExtractCollage(
		not_null<Data::Session*> owner,
		const QVector<TLpageBlock> &items) {
	const auto count = items.size();
	if (count < 2) {
		return {};
	}
	const auto bad = ranges::find_if(items, [](uint32 type) {
		return (type != id_pageBlockPhoto) && (type != id_pageBlockVideo);
	}, &TLpageBlock::type);
	if (bad != items.end()) {
		return {};
	}

	auto result = WebPageCollage();
	result.items.reserve(count);
	for (const auto &item : items) {
		const auto good = item.match([&](const TLDpageBlockPhoto &data) {
			if (const auto found = data.vphoto()) {
				const auto photo = owner->processPhoto(*found);
				if (photo->isNull()) {
					return false;
				}
				result.items.emplace_back(photo);
				return true;
			}
			return false;
		}, [&](const TLDpageBlockVideo &data) {
			if (const auto found = data.vvideo()) {
				const auto document = owner->processDocument(*found);
				if (!document->isVideoFile()) {
					return false;
				}
				result.items.emplace_back(document);
				return true;
			}
			return false;
		}, [](const auto &) -> bool {
			Unexpected("Type of block in Collage.");
		});
		if (!good) {
			return {};
		}
	}
	return result;
}

WebPageCollage ExtractCollage(
		not_null<Data::Session*> owner,
		const TLDwebPageInstantView &data) {
	for (const auto &block : data.vpage_blocks().v) {
		switch (block.type()) {
		case id_pageBlockPhoto:
		case id_pageBlockVideo:
		case id_pageBlockCover:
		case id_pageBlockEmbedded:
		case id_pageBlockEmbeddedPost:
		case id_pageBlockAudio:
			return WebPageCollage();
		case id_pageBlockSlideshow:
			return ExtractCollage(
				owner,
				block.c_pageBlockSlideshow().vpage_blocks().v);
		case id_pageBlockCollage:
			return ExtractCollage(
				owner,
				block.c_pageBlockCollage().vpage_blocks().v);
		default: break;
		}
	}
	return WebPageCollage();
}

WebPageType ParseWebPageType(const TLlinkPreviewType &type, bool hasIV) {
	const auto article = hasIV
		? WebPageType::ArticleWithIV
		: WebPageType::Article;
	return type.match([&](const TLDlinkPreviewTypeAlbum &data) {
		return WebPageType::Album;
	}, [&](const TLDlinkPreviewTypeAnimation &data) {
		return WebPageType::Video;
	}, [&](const TLDlinkPreviewTypeApp &data) {
		return article;
	}, [&](const TLDlinkPreviewTypeArticle &data) {
		return article;
	}, [&](const TLDlinkPreviewTypeAudio &data) {
		return WebPageType::Document;
	}, [&](const TLDlinkPreviewTypeBackground &data) {
		return WebPageType::WallPaper;
	}, [&](const TLDlinkPreviewTypeChannelBoost &data) {
		return WebPageType::ChannelBoost;
	}, [&](const TLDlinkPreviewTypeChat &data) {
		return data.vtype().match([&](const TLDinviteLinkChatTypeChannel &) {
			return data.vcreates_join_request().v
				? WebPageType::ChannelWithRequest
				: WebPageType::Channel;
		}, [&](const auto &) {
			return data.vcreates_join_request().v
				? WebPageType::GroupWithRequest
				: WebPageType::Group;
		});
	}, [&](const TLDlinkPreviewTypeDocument &data) {
		return WebPageType::Document;
	}, [&](const TLDlinkPreviewTypeEmbeddedAnimationPlayer &data) {
		return WebPageType::Video;
	}, [&](const TLDlinkPreviewTypeEmbeddedAudioPlayer &data) {
		return article;
	}, [&](const TLDlinkPreviewTypeEmbeddedVideoPlayer &data) {
		return WebPageType::Video;
	}, [&](const TLDlinkPreviewTypeExternalAudio &data) {
		return article;
	}, [&](const TLDlinkPreviewTypeExternalVideo &data) {
		return WebPageType::Video;
	}, [&](const TLDlinkPreviewTypeInvoice &data) {
		return article;
	}, [&](const TLDlinkPreviewTypeMessage &data) {
		return WebPageType::Message;
	}, [&](const TLDlinkPreviewTypePhoto &data) {
		return WebPageType::Photo;
	}, [&](const TLDlinkPreviewTypePremiumGiftCode &data) {
		return WebPageType::Giftcode;
	}, [&](const TLDlinkPreviewTypeShareableChatFolder &data) {
		return article;
	}, [&](const TLDlinkPreviewTypeSticker &data) {
		return WebPageType::Document;
	}, [&](const TLDlinkPreviewTypeStickerSet &data) {
		return WebPageType::StickerSet;
	}, [&](const TLDlinkPreviewTypeStory &data) {
		return WebPageType::Story;
	}, [&](const TLDlinkPreviewTypeSupergroupBoost &data) {
		return WebPageType::GroupBoost;
	}, [&](const TLDlinkPreviewTypeTheme &data) {
		return WebPageType::Theme;
	}, [&](const TLDlinkPreviewTypeUnsupported &data) {
		return article;
	}, [&](const TLDlinkPreviewTypeUser &data) {
		return data.vis_bot().v ? WebPageType::Bot : WebPageType::User;
	}, [&](const TLDlinkPreviewTypeVideo &data) {
		return WebPageType::Video;
	}, [&](const TLDlinkPreviewTypeVideoChat &data) {
		return data.vis_live_stream().v
			? WebPageType::Livestream
			: WebPageType::VoiceChat;
	}, [&](const TLDlinkPreviewTypeVideoNote &data) {
		return WebPageType::Document;
	}, [&](const TLDlinkPreviewTypeVoiceNote &data) {
		return WebPageType::Document;
	}, [&](const TLDlinkPreviewTypeWebApp &data) {
		return WebPageType::BotApp;
	});
}

[[nodiscard]] FullStoryId ExtractStoryId(const TLlinkPreviewType &type) {
	return type.match([&](const TLDlinkPreviewTypeStory &data) {
		return FullStoryId{
			peerFromTdbChat(data.vstory_sender_chat_id()),
			data.vstory_id().v,
		};
	}, [&](const auto &) {
		return FullStoryId();
	});
}

[[nodiscard]] PhotoData *ExtractPhoto(
		not_null<Data::Session*> owner,
		const TLlinkPreviewType &type) {
	const auto null = (PhotoData*)nullptr;
	const auto photo = [&](const auto &from) {
		using Type = std::decay_t<decltype(from.vphoto())>;
		if constexpr (std::is_same_v<Type, TLphoto>) {
			return owner->processPhoto(from.vphoto()).get();
		} else {
			return from.vphoto()
				? owner->processPhoto(*from.vphoto()).get()
				: nullptr;
		}
	};
	return type.match([&](const TLDlinkPreviewTypeAlbum &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeAnimation &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeApp &data) {
		return photo(data);
	}, [&](const TLDlinkPreviewTypeArticle &data) {
		return photo(data);
	}, [&](const TLDlinkPreviewTypeAudio &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeBackground &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeChannelBoost &data) {
		return photo(data);
	}, [&](const TLDlinkPreviewTypeChat &data) {
		return photo(data);
	}, [&](const TLDlinkPreviewTypeDocument &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeEmbeddedAnimationPlayer &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeEmbeddedAudioPlayer &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeEmbeddedVideoPlayer &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeExternalAudio &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeExternalVideo &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeInvoice &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeMessage &data) {
		return null;
	}, [&](const TLDlinkPreviewTypePhoto &data) {
		return photo(data);
	}, [&](const TLDlinkPreviewTypePremiumGiftCode &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeShareableChatFolder &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeSticker &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeStickerSet &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeStory &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeSupergroupBoost &data) {
		return photo(data);
	}, [&](const TLDlinkPreviewTypeTheme &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeUnsupported &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeUser &data) {
		return photo(data);
	}, [&](const TLDlinkPreviewTypeVideo &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeVideoChat &data) {
		return photo(data);
	}, [&](const TLDlinkPreviewTypeVideoNote &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeVoiceNote &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeWebApp &data) {
		return photo(data);
	});
}

[[nodiscard]] DocumentData *ExtractDocument(
		not_null<Data::Session*> owner,
		const TLlinkPreviewType &type) {
	const auto null = (DocumentData*)nullptr;
	const auto document = [&](const auto &document) {
		return owner->processDocument(document).get();
	};
	return type.match([&](const TLDlinkPreviewTypeAlbum &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeAnimation &data) {
		return document(data.vanimation());
	}, [&](const TLDlinkPreviewTypeApp &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeArticle &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeAudio &data) {
		return document(data.vaudio());
	}, [&](const TLDlinkPreviewTypeBackground &data) {
		return data.vdocument() ? document(*data.vdocument()) : null;
	}, [&](const TLDlinkPreviewTypeChannelBoost &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeChat &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeDocument &data) {
		return document(data.vdocument());
	}, [&](const TLDlinkPreviewTypeEmbeddedAnimationPlayer &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeEmbeddedAudioPlayer &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeEmbeddedVideoPlayer &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeExternalAudio &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeExternalVideo &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeInvoice &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeMessage &data) {
		return null;
	}, [&](const TLDlinkPreviewTypePhoto &data) {
		return null;
	}, [&](const TLDlinkPreviewTypePremiumGiftCode &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeShareableChatFolder &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeSticker &data) {
		return document(data.vsticker());
	}, [&](const TLDlinkPreviewTypeStickerSet &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeStory &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeSupergroupBoost &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeTheme &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeUnsupported &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeUser &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeVideo &data) {
		return document(data.vvideo());
	}, [&](const TLDlinkPreviewTypeVideoChat &data) {
		return null;
	}, [&](const TLDlinkPreviewTypeVideoNote &data) {
		return document(data.vvideo_note());
	}, [&](const TLDlinkPreviewTypeVoiceNote &data) {
		return document(data.vvoice_note());
	}, [&](const TLDlinkPreviewTypeWebApp &data) {
		return null;
	});
}

[[nodiscard]] std::unique_ptr<WebPageStickerSet> ExtractStickerSet(
		not_null<Data::Session*> owner,
		const TLlinkPreviewType &type) {
	return type.match([&](const TLDlinkPreviewTypeStickerSet &data) {
		auto items = std::vector<not_null<DocumentData*>>();
		for (const auto &sticker : data.vstickers().v) {
			if (const auto document = owner->processDocument(sticker)) {
				if (document->sticker()) {
					items.push_back(document);
				}
			}
		}
		if (!items.empty()) {
			auto result = std::make_unique<WebPageStickerSet>();
			const auto info = items.front()->sticker();
			result->isEmoji = info->setType == Data::StickersType::Emoji;
			result->isTextColor = items.front()->emojiUsesTextColor();
			result->items = std::move(items);
			return result;
		}
		return std::unique_ptr<WebPageStickerSet>();
	}, [&](const auto &) {
		return std::unique_ptr<WebPageStickerSet>();
	});
}

[[nodiscard]] int ExtractDuration(const TLlinkPreviewType &type) {
	return type.match([&](const TLDlinkPreviewTypeAnimation &data) {
		return data.vanimation().data().vduration().v;
	}, [&](const TLDlinkPreviewTypeAudio &data) {
		return data.vaudio().data().vduration().v;
	}, [&](const TLDlinkPreviewTypeEmbeddedVideoPlayer &data) {
		return data.vduration().v;
	}, [&](const TLDlinkPreviewTypeVideo &data) {
		return data.vvideo().data().vduration().v;
	}, [&](const TLDlinkPreviewTypeVideoNote &data) {
		return data.vvideo_note().data().vduration().v;
	}, [&](const TLDlinkPreviewTypeVoiceNote &data) {
		return data.vvoice_note().data().vduration().v;
	}, [&](const auto &) {
		return 0;
	});
}

} // namespace

#if 0 // mtp
WebPageType ParseWebPageType(
		const QString &type,
		const QString &embedUrl,
		bool hasIV) {
	if (type == u"video"_q || type == u"gif"_q || !embedUrl.isEmpty()) {
		return WebPageType::Video;
	} else if (type == u"photo"_q) {
		return WebPageType::Photo;
	} else if (type == u"document"_q) {
		return WebPageType::Document;
	} else if (type == u"profile"_q) {
		return WebPageType::Profile;
	} else if (type == u"telegram_background"_q) {
		return WebPageType::WallPaper;
	} else if (type == u"telegram_theme"_q) {
		return WebPageType::Theme;
	} else if (type == u"telegram_story"_q) {
		return WebPageType::Story;
	} else if (type == u"telegram_channel"_q) {
		return WebPageType::Channel;
	} else if (type == u"telegram_channel_request"_q) {
		return WebPageType::ChannelWithRequest;
	} else if (type == u"telegram_megagroup"_q
		|| type == u"telegram_chat"_q) {
		return WebPageType::Group;
	} else if (type == u"telegram_megagroup_request"_q
		|| type == u"telegram_chat_request"_q) {
		return WebPageType::GroupWithRequest;
	} else if (type == u"telegram_album"_q) {
		return WebPageType::Album;
	} else if (type == u"telegram_message"_q) {
		return WebPageType::Message;
	} else if (type == u"telegram_bot"_q) {
		return WebPageType::Bot;
	} else if (type == u"telegram_voicechat"_q) {
		return WebPageType::VoiceChat;
	} else if (type == u"telegram_livestream"_q) {
		return WebPageType::Livestream;
	} else if (type == u"telegram_user"_q) {
		return WebPageType::User;
	} else if (type == u"telegram_botapp"_q) {
		return WebPageType::BotApp;
	} else if (type == u"telegram_channel_boost"_q) {
		return WebPageType::ChannelBoost;
	} else if (type == u"telegram_group_boost"_q) {
		return WebPageType::GroupBoost;
	} else if (type == u"telegram_giftcode"_q) {
		return WebPageType::Giftcode;
	} else if (type == u"telegram_stickerset"_q) {
		return WebPageType::StickerSet;
	} else if (hasIV) {
		return WebPageType::ArticleWithIV;
	} else {
		return WebPageType::Article;
	}
}
#endif

bool IgnoreIv(WebPageType type) {
	return !Iv::ShowButton()
		|| (type == WebPageType::Message)
		|| (type == WebPageType::Album);
}

#if 0 // mtp
WebPageType ParseWebPageType(const MTPDwebPage &page) {
	return ParseWebPageType(
		qs(page.vtype().value_or_empty()),
		page.vembed_url().value_or_empty(),
		!!page.vcached_page());
}

WebPageCollage::WebPageCollage(
	not_null<Data::Session*> owner,
	const MTPDwebPage &data)
: WebPageCollage(ExtractCollage(owner, data)) {
}
#endif

WebPageData::WebPageData(not_null<Data::Session*> owner, const WebPageId &id)
: id(id)
, _owner(owner) {
}

WebPageData::~WebPageData() = default;

WebPageId WebPageData::IdFromTdb(const TLlinkPreview &data) {
	const auto &url = data.data().vurl().v;
	return XXH64(url.data(), url.size() * sizeof(ushort), 0);
}

void WebPageData::setFromTdb(const TLlinkPreview &data) {
	const auto &fields = data.data();
	applyChanges(
		ParseWebPageType(
			fields.vtype(),
			fields.vinstant_view_version().v > 0),
		fields.vurl().v,
		fields.vdisplay_url().v,
		fields.vsite_name().v,
		fields.vtitle().v,
		Api::FormattedTextFromTdb(fields.vdescription()),
		ExtractStoryId(fields.vtype()),
		ExtractPhoto(_owner, fields.vtype()),
		ExtractDocument(_owner, fields.vtype()),
		ExtractStickerSet(_owner, fields.vtype()),
		ExtractDuration(fields.vtype()),
		fields.vauthor().v,
		fields.vhas_large_media().v,
		0);

	if (fields.vinstant_view_version().v > 0 && !_collageRequestId) {
		const auto ignore = IgnoreIv(type);
		const auto owner = _owner;
		const auto sender = &_owner->session().sender();
		const auto requestId = sender->preallocateId();
		const auto finish = [owner, id = id, requestId](
				std::unique_ptr<Iv::Data> iv,
				WebPageCollage &&collage) {
			const auto that = owner->webpage(id);
			if (that->_collageRequestId == requestId) {
				that->_collageRequestId = -1;
				that->setIv(std::move(iv), std::move(collage));
			}
		};
		sender->request(TLgetWebPageInstantView(
			fields.vurl(),
			tl_bool(false) // force_full
		), requestId).done([=](const TLwebPageInstantView &iv) {
			finish(
				(ignore
					? nullptr
					: std::make_unique<Iv::Data>(data.data().vurl().v, iv)),
				ExtractCollage(owner, iv.data()));
		}).fail([finish] {
			finish(nullptr, {});
		}).send();
		_collageRequestId = requestId;
	}
}

Data::Session &WebPageData::owner() const {
	return *_owner;
}

Main::Session &WebPageData::session() const {
	return _owner->session();
}

bool WebPageData::applyChanges(
		WebPageType newType,
		const QString &newUrl,
		const QString &newDisplayUrl,
		const QString &newSiteName,
		const QString &newTitle,
		const TextWithEntities &newDescription,
		FullStoryId newStoryId,
		PhotoData *newPhoto,
		DocumentData *newDocument,
#if 0 // mtp
		WebPageCollage &&newCollage,
		std::unique_ptr<Iv::Data> newIv,
#endif
		std::unique_ptr<WebPageStickerSet> newStickerSet,
		int newDuration,
		const QString &newAuthor,
		bool newHasLargeMedia,
		int newPendingTill) {
	if (newPendingTill != 0
		&& (!url.isEmpty() || failed)
		&& (!pendingTill
			|| pendingTill == newPendingTill
			|| newPendingTill < -1)) {
		return false;
	}

	const auto resultUrl = newUrl;
	const auto resultDisplayUrl = newDisplayUrl;
	const auto possibleSiteName = newSiteName;
	const auto resultTitle = TextUtilities::SingleLine(newTitle);
	const auto resultAuthor = newAuthor;

	const auto viewTitleText = resultTitle.isEmpty()
		? TextUtilities::SingleLine(resultAuthor)
		: resultTitle;
	const auto resultSiteName = [&] {
		if (!possibleSiteName.isEmpty()) {
			return possibleSiteName;
		} else if (!newDescription.text.isEmpty()
			&& viewTitleText.isEmpty()
			&& !resultUrl.isEmpty()) {
			return Iv::SiteNameFromUrl(resultUrl);
		}
		return QString();
	}();
	const auto hasSiteName = !resultSiteName.isEmpty() ? 1 : 0;
	const auto hasTitle = !resultTitle.isEmpty() ? 1 : 0;
	const auto hasDescription = !newDescription.text.isEmpty() ? 1 : 0;
	if (newDocument
#if 0 // mtp
		|| !newCollage.items.empty()
#endif
		|| !newPhoto
		|| (hasSiteName + hasTitle + hasDescription < 2)) {
		newHasLargeMedia = false;
	}

	if (type == newType
		&& url == resultUrl
		&& displayUrl == resultDisplayUrl
		&& siteName == resultSiteName
		&& title == resultTitle
		&& description.text == newDescription.text
		&& storyId == newStoryId
		&& photo == newPhoto
		&& document == newDocument
#if 0 // mtp
		&& collage.items == newCollage.items
		&& (!iv == !newIv)
		&& (!iv || iv->partial() == newIv->partial())
#endif
		&& (!stickerSet == !newStickerSet)
		&& duration == newDuration
		&& author == resultAuthor
		&& hasLargeMedia == (newHasLargeMedia ? 1 : 0)
		&& pendingTill == newPendingTill) {
		return false;
	}
#if 0 // mtp
	if (pendingTill > 0 && newPendingTill <= 0) {
		_owner->session().api().clearWebPageRequest(this);
	}
#endif
	type = newType;
	hasLargeMedia = newHasLargeMedia ? 1 : 0;
	url = resultUrl;
	displayUrl = resultDisplayUrl;
	siteName = resultSiteName;
	title = resultTitle;
	description = newDescription;
	storyId = newStoryId;
	photo = newPhoto;
	document = newDocument;
#if 0 // mtp
	collage = std::move(newCollage);
	iv = std::move(newIv);
#endif
	stickerSet = std::move(newStickerSet);
	duration = newDuration;
	author = resultAuthor;
	pendingTill = newPendingTill;
	++version;

	if (type == WebPageType::WallPaper && document) {
		document->checkWallPaperProperties();
	}

	replaceDocumentGoodThumbnail();

	return true;
}

void WebPageData::replaceDocumentGoodThumbnail() {
	if (document && photo) {
		document->setGoodThumbnailPhoto(photo);
	}
}

void WebPageData::setIv(
		std::unique_ptr<Iv::Data> newIv,
		WebPageCollage &&newCollage) {
	const auto was = version;
	if ((!iv != !newIv)
		|| (iv && iv->partial() != newIv->partial())) {
		iv = std::move(newIv);
		++version;
	}
	if (collage.items != newCollage.items) {
		collage = std::move(newCollage);
		++version;
	}
	if (version != was) {
		_owner->notifyWebPageUpdateDelayed(this);
	}
}

#if 0 // mtp
void WebPageData::ApplyChanges(
		not_null<Main::Session*> session,
		ChannelData *channel,
		const MTPmessages_Messages &result) {
	result.match([&](
			const MTPDmessages_channelMessages &data) {
		if (channel) {
			channel->ptsReceived(data.vpts().v);
			channel->processTopics(data.vtopics());
		} else {
			LOG(("API Error: received messages.channelMessages "
				"when no channel was passed! (WebPageData::ApplyChanges)"));
		}
	}, [&](const auto &) {
	});
	const auto list = result.match([](
			const MTPDmessages_messagesNotModified &) {
		LOG(("API Error: received messages.messagesNotModified! "
			"(WebPageData::ApplyChanges)"));
		return static_cast<const QVector<MTPMessage>*>(nullptr);
	}, [&](const auto &data) {
		session->data().processUsers(data.vusers());
		session->data().processChats(data.vchats());
		return &data.vmessages().v;
	});
	if (!list) {
		return;
	}

	for (const auto &message : *list) {
		message.match([&](const MTPDmessage &data) {
			if (const auto media = data.vmedia()) {
				media->match([&](const MTPDmessageMediaWebPage &data) {
					session->data().processWebpage(data.vwebpage());
				}, [&](const auto &) {
				});
			}
		}, [&](const auto &) {
		});
	}
	session->data().sendWebPageGamePollNotifications();
}
#endif

QString WebPageData::displayedSiteName() const {
	return (document && document->isWallPaper())
		? tr::lng_media_chat_background(tr::now)
		: (document && document->isTheme())
		? tr::lng_media_color_theme(tr::now)
		: siteName;
}

bool WebPageData::computeDefaultSmallMedia() const {
	if (!collage.items.empty()) {
		return false;
	} else if (siteName.isEmpty()
		&& title.isEmpty()
		&& description.empty()
		&& author.isEmpty()) {
		return false;
	} else if (!document
		&& photo
		&& type != WebPageType::Photo
		&& type != WebPageType::Document
		&& type != WebPageType::Story
		&& type != WebPageType::Video) {
		if (type == WebPageType::Profile) {
			return true;
		} else if (siteName == u"Twitter"_q
			|| siteName == u"Facebook"_q
			|| type == WebPageType::ArticleWithIV) {
			return false;
		} else {
			return true;
		}
	}
	return false;
}

bool WebPageData::suggestEnlargePhoto() const {
	return !siteName.isEmpty() || !title.isEmpty() || !description.empty();
}
