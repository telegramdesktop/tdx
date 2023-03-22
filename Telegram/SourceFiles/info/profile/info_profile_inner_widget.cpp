/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_inner_widget.h"

#include "info/info_memento.h"
#include "info/info_controller.h"
#include "info/profile/info_profile_widget.h"
#include "info/profile/info_profile_text.h"
#include "info/profile/info_profile_values.h"
#include "info/profile/info_profile_cover.h"
#include "info/profile/info_profile_icon.h"
#include "info/profile/info_profile_members.h"
#include "info/profile/info_profile_actions.h"
#include "info/media/info_media_buttons.h"
#include "boxes/abstract_box.h"
#include "boxes/add_contact_box.h"
#include "data/data_changes.h"
#include "data/data_forum_topic.h"
#include "data/data_photo.h"
#include "data/data_file_origin.h"
#include "ui/boxes/confirm_box.h"
#include "mainwidget.h"
#include "main/main_session.h"
#include "apiwrap.h"
#include "api/api_peer_photo.h"
#include "window/main_window.h"
#include "window/window_session_controller.h"
#include "storage/storage_shared_media.h"
#include "lang/lang_keys.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/shadow.h"
#include "ui/widgets/box_content_divider.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/ui_utility.h"
#include "data/data_channel.h"
#include "data/data_shared_media.h"
#include "styles/style_info.h"
#include "styles/style_boxes.h"

#include "data/data_secret_chat.h"

namespace Info {
namespace Profile {
namespace {

void SetupKeyHash(
		not_null<Ui::VerticalLayout*> container,
		not_null<SecretChatData*> secretChat) {
	const auto skip = st::infoProfileCover.photoLeft;
	const auto wrap = container->add(
		object_ptr<Ui::RpWidget>(container),
		{ skip, 0, skip, 0 });
	static const auto kColors = std::array{
		QColor(0xFF, 0xFF, 0xFF),
		QColor(0xD5, 0xE6, 0xF3),
		QColor(0x2D, 0x57, 0x75),
		QColor(0x2F, 0x99, 0xC9),
	};
	struct State {
		std::vector<std::vector<int>> indices;
	};
	const auto state = wrap->lifetime().make_state<State>();

	secretChat->keyHashValue(
	) | rpl::start_with_next([=](QByteArray key) {
		Expects(key.isEmpty() || key.size() == 36);

		const auto guard = gsl::finally([&] { wrap->update(); });
		if (key.isEmpty()) {
			state->indices.clear();
			return;
		}
		state->indices.resize(12, std::vector<int>(12));
		auto index = 0;
		const auto put = [&](int value) {
			const auto row = index / 12;
			const auto column = index % 12;
			state->indices[row][column] = value;
			++index;
		};
		for (auto i = 0; i != 36; ++i) {
			const auto value = uint8_t(key[i]);
			const auto push = [&](int shift) {
				put((value >> (shift * 2)) & 0x03);
			};
			for (auto j = 0; j != 4; ++j) {
				push(j);
			}
		}
	}, wrap->lifetime());

	wrap->widthValue(
	) | rpl::filter(
		rpl::mappers::_1 > 0
	) | rpl::start_with_next([=](int width) {
		wrap->resize(wrap->width(), wrap->width());
	}, wrap->lifetime());

	wrap->paintRequest(
	) | rpl::start_with_next([=] {
		if (state->indices.empty()) {
			return;
		}
		auto p = QPainter(wrap);
		const auto width = wrap->width();
		const auto height = wrap->height();
		const auto w = width / float64(state->indices.size());
		const auto h = height / float64(state->indices.front().size());
		const auto round = [](float64 value) {
			return int(base::SafeRound(value));
		};
		auto top = 0.;
		for (const auto &row : state->indices) {
			auto left = 0.;
			for (const auto index : row) {
				const auto x = round(left);
				const auto y = round(top);
				p.fillRect(
					QRect(x, y, round(left + w) - x, round(top + h) - y),
					kColors[index]);
				left += w;
			}
			top += h;
		}
	}, wrap->lifetime());
}

} // namespace

InnerWidget::InnerWidget(
	QWidget *parent,
	not_null<Controller*> controller,
	Origin origin)
: RpWidget(parent)
, _controller(controller)
, _peer(_controller->key().peer())
, _migrated(_controller->migrated())
, _topic(_controller->key().topic())
, _content(setupContent(this, origin)) {
	_content->heightValue(
	) | rpl::start_with_next([this](int height) {
		if (!_inResize) {
			resizeToWidth(width());
			updateDesiredHeight();
		}
	}, lifetime());
}

object_ptr<Ui::RpWidget> InnerWidget::setupContent(
		not_null<RpWidget*> parent,
		Origin origin) {
	auto result = object_ptr<Ui::VerticalLayout>(parent);
	if (const auto user = _peer->asUser()) {
		user->session().changes().peerFlagsValue(
			user,
			Data::PeerUpdate::Flag::FullInfo
		) | rpl::start_with_next([=] {
			auto &photos = user->session().api().peerPhoto();
			if (const auto original = photos.nonPersonalPhoto(user)) {
				// Preload it for the edit contact box.
				_nonPersonalView = original->createMediaView();
				const auto id = peerToUser(user->id);
				original->load(Data::FileOriginFullUser{ id });
			}
		}, lifetime());
	}
	_cover = _topic
		? result->add(object_ptr<Cover>(
			result,
			_controller->parentController(),
			_topic))
		: result->add(object_ptr<Cover>(
			result,
			_controller->parentController(),
			_peer));
	_cover->showSection(
	) | rpl::start_with_next([=](Section section) {
		_controller->showSection(_topic
			? std::make_shared<Info::Memento>(_topic, section)
			: std::make_shared<Info::Memento>(_peer, section));
	}, _cover->lifetime());
	_cover->setOnlineCount(rpl::single(0));
	if (_topic) {
		if (_topic->creating()) {
			return result;
		}
		result->add(SetupDetails(_controller, parent, _topic));
	} else {
		result->add(SetupDetails(_controller, parent, _peer, origin));
	}
	result->add(setupSharedMedia(result.data()));
	if (_topic) {
		return result;
	}
	{
		auto buttons = SetupChannelMembersAndManage(
			_controller,
			result.data(),
			_peer);
		if (buttons) {
			result->add(std::move(buttons));
		}
	}
	if (auto actions = SetupActions(_controller, result.data(), _peer)) {
		result->add(object_ptr<Ui::BoxContentDivider>(result));
		result->add(std::move(actions));
	}
	if (_peer->isChat() || _peer->isMegagroup()) {
		setupMembers(result.data());
	}
	if (const auto secretChat = _peer->asSecretChat()) {
		SetupKeyHash(result.data(), secretChat);
	}
	return result;
}

void InnerWidget::setupMembers(not_null<Ui::VerticalLayout*> container) {
	auto wrap = container->add(object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		container,
		object_ptr<Ui::VerticalLayout>(container)));
	const auto inner = wrap->entity();
	inner->add(object_ptr<Ui::BoxContentDivider>(inner));
	_members = inner->add(object_ptr<Members>(inner, _controller));
	_members->scrollToRequests(
	) | rpl::start_with_next([this](Ui::ScrollToRequest request) {
		auto min = (request.ymin < 0)
			? request.ymin
			: MapFrom(this, _members, QPoint(0, request.ymin)).y();
		auto max = (request.ymin < 0)
			? MapFrom(this, _members, QPoint()).y()
			: (request.ymax < 0)
			? request.ymax
			: MapFrom(this, _members, QPoint(0, request.ymax)).y();
		_scrollToRequests.fire({ min, max });
	}, _members->lifetime());
	_cover->setOnlineCount(_members->onlineCountValue());

	using namespace rpl::mappers;
	wrap->toggleOn(
		_members->fullCountValue() | rpl::map(_1 > 0),
		anim::type::instant);
}

object_ptr<Ui::RpWidget> InnerWidget::setupSharedMedia(
		not_null<RpWidget*> parent) {
	using namespace rpl::mappers;
	using MediaType = Media::Type;

	auto content = object_ptr<Ui::VerticalLayout>(parent);
	auto tracker = Ui::MultiSlideTracker();
	auto addMediaButton = [&](
			MediaType type,
			const style::icon &icon) {
		auto result = Media::AddButton(
			content,
			_controller,
			_peer,
			_topic ? _topic->rootId() : 0,
			_migrated,
			type,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};
	auto addCommonGroupsButton = [&](
			not_null<UserData*> user,
			const style::icon &icon) {
		auto result = Media::AddCommonGroupsButton(
			content,
			_controller,
			user,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};
	const auto addSimilarChannelsButton = [&](
			not_null<ChannelData*> channel,
			const style::icon &icon) {
		auto result = Media::AddSimilarChannelsButton(
			content,
			_controller,
			channel,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};
	auto addStoriesButton = [&](
			not_null<PeerData*> peer,
			const style::icon &icon) {
		if (peer->isChat()) {
			return;
		}
		auto result = Media::AddStoriesButton(
			content,
			_controller,
			peer,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};
	auto addSavedSublistButton = [&](
			not_null<PeerData*> peer,
			const style::icon &icon) {
		auto result = Media::AddSavedSublistButton(
			content,
			_controller,
			peer,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};
	auto addPeerGiftsButton = [&](
			not_null<UserData*> user,
			const style::icon &icon) {
		auto result = Media::AddPeerGiftsButton(
			content,
			_controller,
			user,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};

	if (!_topic) {
		addStoriesButton(_peer, st::infoIconMediaStories);
		if (const auto user = _peer->asUser()) {
			addPeerGiftsButton(user, st::infoIconMediaGifts);
		}
		addSavedSublistButton(_peer, st::infoIconMediaSaved);
	}
	addMediaButton(MediaType::Photo, st::infoIconMediaPhoto);
	addMediaButton(MediaType::Video, st::infoIconMediaVideo);
	addMediaButton(MediaType::File, st::infoIconMediaFile);
	addMediaButton(MediaType::MusicFile, st::infoIconMediaAudio);
	addMediaButton(MediaType::Link, st::infoIconMediaLink);
	addMediaButton(MediaType::RoundVoiceFile, st::infoIconMediaVoice);
	addMediaButton(MediaType::GIF, st::infoIconMediaGif);
	if (const auto user = _peer->asUser()) {
		addCommonGroupsButton(user, st::infoIconMediaGroup);
	} else if (const auto channel = _peer->asChannel()) {
		addSimilarChannelsButton(channel, st::infoIconMediaChannel);
	}

	auto result = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent)
	);

	result->setDuration(
		st::infoSlideDuration
	)->toggleOn(
		tracker.atLeastOneShownValue()
	);

	auto layout = result->entity();

	layout->add(object_ptr<Ui::BoxContentDivider>(layout));
	layout->add(object_ptr<Ui::FixedHeightWidget>(
		layout,
		st::infoSharedMediaBottomSkip)
	)->setAttribute(Qt::WA_TransparentForMouseEvents);
	layout->add(std::move(content));
	layout->add(object_ptr<Ui::FixedHeightWidget>(
		layout,
		st::infoSharedMediaBottomSkip)
	)->setAttribute(Qt::WA_TransparentForMouseEvents);

	_sharedMediaWrap = result;
	return result;
}

int InnerWidget::countDesiredHeight() const {
	return _content->height() + (_members
		? (_members->desiredHeight() - _members->height())
		: 0);
}

void InnerWidget::visibleTopBottomUpdated(
		int visibleTop,
		int visibleBottom) {
	setChildVisibleTopBottom(_content, visibleTop, visibleBottom);
}

void InnerWidget::saveState(not_null<Memento*> memento) {
	if (_members) {
		memento->setMembersState(_members->saveState());
	}
}

void InnerWidget::restoreState(not_null<Memento*> memento) {
	if (_members) {
		_members->restoreState(memento->membersState());
	}
	if (_sharedMediaWrap) {
		_sharedMediaWrap->finishAnimating();
	}
}

rpl::producer<Ui::ScrollToRequest> InnerWidget::scrollToRequests() const {
	return _scrollToRequests.events();
}

rpl::producer<int> InnerWidget::desiredHeightValue() const {
	return _desiredHeight.events_starting_with(countDesiredHeight());
}

int InnerWidget::resizeGetHeight(int newWidth) {
	_inResize = true;
	auto guard = gsl::finally([&] { _inResize = false; });

	_content->resizeToWidth(newWidth);
	_content->moveToLeft(0, 0);
	updateDesiredHeight();
	return _content->heightNoMargins();
}

} // namespace Profile
} // namespace Info
