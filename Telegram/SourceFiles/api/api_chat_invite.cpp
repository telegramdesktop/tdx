/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_chat_invite.h"

#include "apiwrap.h"
#include "boxes/premium_limits_box.h"
#include "core/application.h"
#include "data/components/credits.h"
#include "data/data_channel.h"
#include "data/data_file_origin.h"
#include "data/data_forum.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "info/channel_statistics/boosts/giveaway/boost_badge.h"
#include "info/profile/info_profile_badge.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/settings_credits_graphics.h"
#include "ui/boxes/confirm_box.h"
#include "ui/controls/userpic_button.h"
#include "ui/effects/premium_graphics.h"
#include "ui/effects/premium_stars_colored.h"
#include "ui/empty_userpic.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_credits.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_premium.h"

#include "tdb/tdb_tl_scheme.h"
#include "base/unixtime.h"

using namespace Tdb;

namespace Api {

namespace {

void SubmitChatInvite(
		base::weak_ptr<Window::SessionController> weak,
		not_null<Main::Session*> session,
		const QString &hash,
		bool isGroup) {
#if 0 // mtp
	session->api().request(MTPmessages_ImportChatInvite(
		MTP_string(hash)
	)).done([=](const MTPUpdates &result) {
		session->api().applyUpdates(result);
		const auto strongController = weak.get();
		if (!strongController) {
			return;
		}

		strongController->hideLayer();
		const auto handleChats = [&](const MTPVector<MTPChat> &chats) {
			if (chats.v.isEmpty()) {
				return;
			}
			const auto peerId = chats.v[0].match([](const MTPDchat &data) {
				return peerFromChat(data.vid().v);
			}, [](const MTPDchannel &data) {
				return peerFromChannel(data.vid().v);
			}, [](auto&&) {
				return PeerId(0);
			});
			if (const auto peer = session->data().peerLoaded(peerId)) {
				// Shows in the primary window anyway.
				strongController->showPeerHistory(
					peer,
					Window::SectionShow::Way::Forward);
			}
		};
		result.match([&](const MTPDupdates &data) {
			handleChats(data.vchats());
		}, [&](const MTPDupdatesCombined &data) {
			handleChats(data.vchats());
		}, [&](auto &&) {
			LOG(("API Error: unexpected update cons %1 "
				"(ApiWrap::importChatInvite)").arg(result.type()));
		});
	}).fail([=](const MTP::Error &error) {
		const auto &type = error.type();
#endif
	session->sender().request(TLjoinChatByInviteLink(
		tl_string(hash)
	)).done([=](const TLchat &result) {
		const auto peer = session->data().processPeer(result);
		const auto strong = weak.get();
		if (!strong) {
			return;
		}

		strong->hideLayer();
		strong->showPeerHistory(peer, Window::SectionShow::Way::Forward);
	}).fail([=](const Error &error) {
		const auto &type = error.message;

		const auto strongController = weak.get();
		if (!strongController) {
			return;
		} else if (type == u"CHANNELS_TOO_MUCH"_q) {
			strongController->show(
				Box(ChannelsLimitBox, &strongController->session()));
			return;
		}

		strongController->hideLayer();
		strongController->showToast([&] {
			if (type == u"INVITE_REQUEST_SENT"_q) {
				return isGroup
					? tr::lng_group_request_sent(tr::now)
					: tr::lng_group_request_sent_channel(tr::now);
			} else if (type == u"USERS_TOO_MUCH"_q) {
				return tr::lng_group_invite_no_room(tr::now);
			} else {
				return tr::lng_group_invite_bad_link(tr::now);
			}
		}(), ApiWrap::kJoinErrorDuration);
	}).send();
}

void ConfirmSubscriptionBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		const QString &hash,
#if 0 // mtp
		const MTPDchatInvite *data) {
#endif
		const TLDchatInviteLinkInfo *data) {
	box->setWidth(st::boxWideWidth);
#if 0 // mtp
	const auto amount = data->vsubscription_pricing()->data().vamount().v;
	const auto formId = data->vsubscription_form_id()->v;
	const auto name = qs(data->vtitle());
	const auto maybePhoto = session->data().processPhoto(data->vphoto());
	const auto photo = maybePhoto->isNull() ? nullptr : maybePhoto.get();
#endif
	const auto &pricing = data->vsubscription_info()->data().vpricing();
	const auto amount = pricing.data().vstar_count().v;
	const auto formId = data->vsubscription_info()->data().vform_id().v;
	const auto name = qs(data->vtitle());
	const auto photo = data->vphoto()
		? session->data().processSmallPhoto(*data->vphoto()).get()
		: nullptr;

	struct State final {
		std::shared_ptr<Data::PhotoMedia> photoMedia;
		std::unique_ptr<Ui::EmptyUserpic> photoEmpty;

		std::optional<MTP::Sender> api;
		Ui::RpWidget* saveButton = nullptr;
		rpl::variable<bool> loading;
	};
	const auto state = box->lifetime().make_state<State>();

	const auto content = box->verticalLayout();

	Ui::AddSkip(content, st::confirmInvitePhotoTop);
	const auto userpicWrap = content->add(
		object_ptr<Ui::CenterWrap<>>(
			content,
			object_ptr<Ui::RpWidget>(content)));
	const auto userpic = userpicWrap->entity();
	const auto photoSize = st::confirmInvitePhotoSize;
	userpic->resize(Size(photoSize));
	const auto options = Images::Option::RoundCircle;
	userpic->paintRequest(
	) | rpl::start_with_next([=, small = Data::PhotoSize::Small] {
		auto p = QPainter(userpic);
		if (state->photoMedia) {
			if (const auto image = state->photoMedia->image(small)) {
				p.drawPixmap(
					0,
					0,
					image->pix(Size(photoSize), { .options = options }));
			}
		} else if (state->photoEmpty) {
			state->photoEmpty->paintCircle(
				p,
				0,
				0,
				userpic->width(),
				photoSize);
		}
	}, userpicWrap->lifetime());
	userpicWrap->setAttribute(Qt::WA_TransparentForMouseEvents);
	if (photo) {
		state->photoMedia = photo->createMediaView();
		state->photoMedia->wanted(Data::PhotoSize::Small, Data::FileOrigin());
		if (!state->photoMedia->image(Data::PhotoSize::Small)) {
			session->downloaderTaskFinished(
			) | rpl::start_with_next([=] {
				userpic->update();
			}, userpicWrap->entity()->lifetime());
		}
	} else {
		state->photoEmpty = std::make_unique<Ui::EmptyUserpic>(
			Ui::EmptyUserpic::UserpicColor(0),
			name);
	}
	Ui::AddSkip(content);
	Ui::AddSkip(content);

	{
		const auto widget = Ui::CreateChild<Ui::RpWidget>(content);
		using ColoredMiniStars = Ui::Premium::ColoredMiniStars;
		const auto stars = widget->lifetime().make_state<ColoredMiniStars>(
			widget,
			false,
			Ui::Premium::MiniStars::Type::BiStars);
		stars->setColorOverride(Ui::Premium::CreditsIconGradientStops());
		widget->resize(
			st::boxWideWidth - photoSize,
			photoSize * 2);
		content->sizeValue(
		) | rpl::start_with_next([=](const QSize &size) {
			widget->moveToLeft(photoSize / 2, 0);
			const auto starsRect = Rect(widget->size());
			stars->setPosition(starsRect.topLeft());
			stars->setSize(starsRect.size());
			widget->lower();
		}, widget->lifetime());
		widget->paintRequest(
		) | rpl::start_with_next([=](const QRect &r) {
			auto p = QPainter(widget);
			p.fillRect(r, Qt::transparent);
			stars->paint(p);
		}, widget->lifetime());
	}

	box->addRow(
		object_ptr<Ui::CenterWrap<Ui::FlatLabel>>(
			box,
			object_ptr<Ui::FlatLabel>(
				box,
				tr::lng_channel_invite_subscription_title(),
				st::inviteLinkSubscribeBoxTitle)));
	box->addRow(
		object_ptr<Ui::CenterWrap<Ui::FlatLabel>>(
			box,
			object_ptr<Ui::FlatLabel>(
				box,
				tr::lng_channel_invite_subscription_about(
					lt_channel,
					rpl::single(Ui::Text::Bold(name)),
					lt_price,
					tr::lng_credits_summary_options_credits(
						lt_count,
						rpl::single(amount) | tr::to_count(),
						Ui::Text::Bold),
					Ui::Text::WithEntities),
				st::inviteLinkSubscribeBoxAbout)));
	Ui::AddSkip(content);
	box->addRow(
		object_ptr<Ui::CenterWrap<Ui::FlatLabel>>(
			box,
			object_ptr<Ui::FlatLabel>(
				box,
				tr::lng_channel_invite_subscription_terms(
					lt_link,
					rpl::combine(
						tr::lng_paid_react_agree_link(),
						tr::lng_group_invite_subscription_about_url()
					) | rpl::map([](const QString &text, const QString &url) {
						return Ui::Text::Link(text, url);
					}),
					Ui::Text::RichLangValue),
				st::inviteLinkSubscribeBoxTerms)));

	{
		const auto balance = Settings::AddBalanceWidget(
			content,
			session->credits().balanceValue(),
			true);
		session->credits().load(true);

		rpl::combine(
			balance->sizeValue(),
			content->sizeValue()
		) | rpl::start_with_next([=](const QSize &, const QSize &) {
			balance->moveToRight(
				st::creditsHistoryRightSkip * 2,
				st::creditsHistoryRightSkip);
			balance->update();
		}, balance->lifetime());
	}

	const auto sendCredits = [=, weak = Ui::MakeWeak(box)] {
		const auto show = box->uiShow();
		const auto buttonWidth = state->saveButton
			? state->saveButton->width()
			: 0;
		state->api->request(TLsendPaymentForm(
			tl_inputInvoiceTelegram(
				tl_telegramPaymentPurposeJoinChat(tl_string(hash))),
			tl_int64(formId),
			tl_string(), // order_info_id
			tl_string(), // shipping_option_id
			null, // credentials
			tl_int53(0) // tip_amount
		)).done([=](const TLpaymentResult &result) {
#if 0 // mtp
		state->api->request(
			MTPpayments_SendStarsForm(
				MTP_long(formId),
				MTP_inputInvoiceChatInviteSubscription(MTP_string(hash)))
		).done([=](const MTPpayments_PaymentResult &result) {
#endif
			state->api = std::nullopt;
			state->loading.force_assign(false);
#if 0 // mtp
			result.match([&](const MTPDpayments_paymentResult &data) {
				session->api().applyUpdates(data.vupdates());
			}, [](const MTPDpayments_paymentVerificationNeeded &data) {
			});
#endif
			if (weak) {
				box->closeBox();
			}
#if 0 // mtp
		}).fail([=](const MTP::Error &error) {
			const auto id = error.type();
#endif

		}).fail([=](const Error &error) {
			const auto id = error.message;
			if (weak) {
				state->api = std::nullopt;
			}
			show->showToast(id);
			state->loading.force_assign(false);
		}).send();
		if (state->saveButton) {
			state->saveButton->resizeToWidth(buttonWidth);
		}
	};

	auto confirmText = tr::lng_channel_invite_subscription_button();
	state->saveButton = box->addButton(std::move(confirmText), [=] {
		if (state->api) {
			return;
		}
		state->api.emplace(&session->mtp());
		state->loading.force_assign(true);

		const auto done = [=](Settings::SmallBalanceResult result) {
			if (result == Settings::SmallBalanceResult::Success
				|| result == Settings::SmallBalanceResult::Already) {
				sendCredits();
			} else {
				state->api = std::nullopt;
				state->loading.force_assign(false);
			}
		};
		Settings::MaybeRequestBalanceIncrease(
			Main::MakeSessionShow(box->uiShow(), session),
			amount,
			Settings::SmallBalanceSubscription{ .name = name },
			done);
	});

	if (const auto saveButton = state->saveButton) {
		using namespace Info::Statistics;
		const auto loadingAnimation = InfiniteRadialAnimationWidget(
			saveButton,
			saveButton->height() / 2,
			&st::editStickerSetNameLoading);
		AddChildToWidgetCenter(saveButton, loadingAnimation);
		loadingAnimation->showOn(
			state->loading.value() | rpl::map(rpl::mappers::_1));
	}
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace

void CheckChatInvite(
		not_null<Window::SessionController*> controller,
		const QString &hash,
		ChannelData *invitePeekChannel,
		Fn<void()> loaded) {
	const auto session = &controller->session();
	const auto weak = base::make_weak(controller);
	session->api().checkChatInvite(hash, [=](
			const TLchatInviteLinkInfo &result) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		Core::App().hideMediaView();
		const auto &data = result.data();
		const auto id = data.vchat_id().v
			? peerFromTdbChat(data.vchat_id())
			: PeerId();
		const auto peer = id ? session->data().peer(id).get() : nullptr;
		if (peer) {
			if (const auto channel = peer->asChannel()) {
				const auto peek = data.vaccessible_for().v;
				if (peek > 0) {
					const auto now = base::unixtime::now();
					channel->setInvitePeek(hash, now + peek);
				} else {
					channel->clearInvitePeek();
				}
			}
			strong->showPeerHistory(
				peer,
				Window::SectionShow::Way::Forward);
		} else {
			const auto isGroup = data.vtype().match([](
					const TLDinviteLinkChatTypeBasicGroup&) {
				return true;
			}, [&](const TLDinviteLinkChatTypeSupergroup&) {
				return true;
			}, [](const TLDinviteLinkChatTypeChannel&) {
				return false;
			});
			const auto hasPricing = data.vsubscription_info().has_value();
			const auto canRefulfill = hasPricing
				&& data.vsubscription_info()->data().vcan_reuse().v;
			const auto formId = hasPricing
				? data.vsubscription_info()->data().vform_id().v
				: 0;
			if (hasPricing
				&& !canRefulfill
				&& !formId) {
				strong->uiShow()->showToast(
					tr::lng_confirm_phone_link_invalid(tr::now));
				return;
			}
			const auto box = (hasPricing && !canRefulfill)
				? strong->show(Box(
					ConfirmSubscriptionBox,
					session,
					hash,
					&data))
				: strong->show(Box<ConfirmInviteBox>(
					session,
					data,
					invitePeekChannel,
					[=] { SubmitChatInvite(weak, session, hash, isGroup); }));
			if (invitePeekChannel) {
				box->boxClosing(
				) | rpl::filter([=] {
					return !invitePeekChannel->amIn();
				}) | rpl::start_with_next([=] {
					if (const auto strong = weak.get()) {
						strong->clearSectionStack(Window::SectionShow(
							Window::SectionShow::Way::ClearStack,
							anim::type::normal,
							anim::activation::background));
					}
				}, box->lifetime());
			}
		}
	}, [=](const Error &error) {
		if (error.code != 400) {
			return;
		}
		Core::App().hideMediaView();
		if (const auto strong = weak.get()) {
			strong->show(Ui::MakeInformBox(tr::lng_group_invite_bad_link()));
		}
	});
#if 0 // mtp
	session->api().checkChatInvite(hash, [=](const MTPChatInvite &result) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		if (loaded) {
			loaded();
		}
		Core::App().hideMediaView();
		const auto show = [&](not_null<PeerData*> chat) {
			const auto way = Window::SectionShow::Way::Forward;
			if (const auto forum = chat->forum()) {
				strong->showForum(forum, way);
			} else {
				strong->showPeerHistory(chat, way);
			}
		};
		result.match([=](const MTPDchatInvite &data) {
			const auto isGroup = !data.is_broadcast();
			const auto hasPricing = !!data.vsubscription_pricing();
			const auto canRefulfill = data.is_can_refulfill_subscription();
			if (hasPricing
				&& !canRefulfill
				&& !data.vsubscription_form_id()) {
				strong->uiShow()->showToast(
					tr::lng_confirm_phone_link_invalid(tr::now));
				return;
			}
			const auto box = (hasPricing && !canRefulfill)
				? strong->show(Box(
					ConfirmSubscriptionBox,
					session,
					hash,
					&data))
				: strong->show(Box<ConfirmInviteBox>(
					session,
					data,
					invitePeekChannel,
					[=] { SubmitChatInvite(weak, session, hash, isGroup); }));
			if (invitePeekChannel) {
				box->boxClosing(
				) | rpl::filter([=] {
					return !invitePeekChannel->amIn();
				}) | rpl::start_with_next([=] {
					if (const auto strong = weak.get()) {
						strong->clearSectionStack(Window::SectionShow(
							Window::SectionShow::Way::ClearStack,
							anim::type::normal,
							anim::activation::background));
					}
				}, box->lifetime());
			}
		}, [=](const MTPDchatInviteAlready &data) {
			if (const auto chat = session->data().processChat(data.vchat())) {
				if (const auto channel = chat->asChannel()) {
					channel->clearInvitePeek();
				}
				show(chat);
			}
		}, [=](const MTPDchatInvitePeek &data) {
			if (const auto chat = session->data().processChat(data.vchat())) {
				if (const auto channel = chat->asChannel()) {
					channel->setInvitePeek(hash, data.vexpires().v);
					show(chat);
				}
			}
		});
	}, [=](const MTP::Error &error) {
		if (error.code() != 400) {
			return;
		}
		Core::App().hideMediaView();
		if (const auto strong = weak.get()) {
			strong->show(Ui::MakeInformBox(tr::lng_group_invite_bad_link()));
		}
	});
#endif
}

} // namespace Api

struct ConfirmInviteBox::Participant {
	not_null<UserData*> user;
	Ui::PeerUserpicView userpic;
};

#if 0 // mtp
ConfirmInviteBox::ConfirmInviteBox(
	QWidget*,
	not_null<Main::Session*> session,
	const MTPDchatInvite &data,
	ChannelData *invitePeekChannel,
	Fn<void()> submit)
: ConfirmInviteBox(
	session,
	Parse(session, data),
	invitePeekChannel,
	std::move(submit)) {
}
#endif
ConfirmInviteBox::ConfirmInviteBox(
	QWidget*,
	not_null<Main::Session*> session,
	const TLDchatInviteLinkInfo &data,
	ChannelData *invitePeekChannel,
	Fn<void()> submit)
: ConfirmInviteBox(
	session,
	Parse(session, data),
	invitePeekChannel,
	std::move(submit)) {
}

ConfirmInviteBox::ConfirmInviteBox(
	not_null<Main::Session*> session,
	ChatInvite &&invite,
	ChannelData *invitePeekChannel,
	Fn<void()> submit)
: _session(session)
, _submit(std::move(submit))
, _title(this, st::confirmInviteTitle)
, _badge(std::make_unique<Info::Profile::Badge>(
	this,
	st::infoPeerBadge,
	_session,
	rpl::single(Info::Profile::Badge::Content{ BadgeForInvite(invite) }),
	nullptr,
	[=] { return false; }))
, _status(this, st::confirmInviteStatus)
, _about(this, st::confirmInviteAbout)
, _aboutRequests(this, st::confirmInviteStatus)
, _participants(std::move(invite.participants))
, _isChannel(invite.isChannel && !invite.isMegagroup)
, _requestApprove(invite.isRequestNeeded) {
	const auto count = invite.participantsCount;
	const auto status = [&] {
		return invitePeekChannel
			? tr::lng_channel_invite_private(tr::now)
			: (!_participants.empty() && _participants.size() < count)
			? tr::lng_group_invite_members(tr::now, lt_count, count)
			: (count > 0 && _isChannel)
			? tr::lng_chat_status_subscribers(
				tr::now,
				lt_count_decimal,
				count)
			: (count > 0)
			? tr::lng_chat_status_members(tr::now, lt_count_decimal, count)
			: _isChannel
			? tr::lng_channel_status(tr::now)
			: tr::lng_group_status(tr::now);
	}();
	_title->setText(invite.title);
	_status->setText(status);
	if (!invite.about.isEmpty()) {
		_about->setText(invite.about);
	} else {
		_about.destroy();
	}
	if (_requestApprove) {
		_aboutRequests->setText(_isChannel
			? tr::lng_group_request_about_channel(tr::now)
			: tr::lng_group_request_about(tr::now));
	} else {
		_aboutRequests.destroy();
	}

	if (invite.photo) {
		_photo = invite.photo->createMediaView();
		_photo->wanted(Data::PhotoSize::Small, Data::FileOrigin());
		if (!_photo->image(Data::PhotoSize::Small)) {
			_session->downloaderTaskFinished(
			) | rpl::start_with_next([=] {
				update();
			}, lifetime());
		}
	} else {
		_photoEmpty = std::make_unique<Ui::EmptyUserpic>(
			Ui::EmptyUserpic::UserpicColor(0),
			invite.title);
	}
}

ConfirmInviteBox::~ConfirmInviteBox() = default;

#if 0 // mtp
ConfirmInviteBox::ChatInvite ConfirmInviteBox::Parse(
		not_null<Main::Session*> session,
		const MTPDchatInvite &data) {
	auto participants = std::vector<Participant>();
	if (const auto list = data.vparticipants()) {
		participants.reserve(list->v.size());
		for (const auto &participant : list->v) {
			if (const auto user = session->data().processUser(participant)) {
				participants.push_back(Participant{ user });
			}
		}
	}
	const auto photo = session->data().processPhoto(data.vphoto());
	return {
		.title = qs(data.vtitle()),
		.about = data.vabout().value_or_empty(),
		.photo = (photo->isNull() ? nullptr : photo.get()),
		.participantsCount = data.vparticipants_count().v,
		.participants = std::move(participants),
		.isPublic = data.is_public(),
		.isChannel = data.is_channel(),
		.isMegagroup = data.is_megagroup(),
		.isBroadcast = data.is_broadcast(),
		.isRequestNeeded = data.is_request_needed(),
		.isFake = data.is_fake(),
		.isScam = data.is_scam(),
		.isVerified = data.is_verified(),
	};
}
#endif
ConfirmInviteBox::ChatInvite ConfirmInviteBox::Parse(
		not_null<Main::Session*> session,
		const TLDchatInviteLinkInfo &data) {
	auto participants = std::vector<Participant>();
	const auto &list = data.vmember_user_ids().v;
	participants.reserve(list.size());
	for (const auto &userId : list) {
		participants.push_back(Participant{
			session->data().user(UserId(userId.v))
		});
	}
	auto result = ChatInvite{
		.title = data.vtitle().v,
		.about = data.vdescription().v,
		.photo = (data.vphoto()
			? session->data().processSmallPhoto(*data.vphoto()).get()
			: nullptr),
		.participantsCount = data.vmember_count().v,
		.participants = std::move(participants),
		.isPublic = data.vis_public().v,
		.isRequestNeeded = data.vcreates_join_request().v,
		.isFake = data.vis_fake().v,
		.isScam = data.vis_scam().v,
		.isVerified = data.vis_verified().v,
	};
	data.vtype().match([](const TLDinviteLinkChatTypeBasicGroup&) {
	}, [&](const TLDinviteLinkChatTypeSupergroup&) {
		result.isChannel = true;
		result.isMegagroup = true;
	}, [&](const TLDinviteLinkChatTypeChannel &data) {
		result.isChannel = true;
		result.isBroadcast = true;
	});
	return result;
}

[[nodiscard]] Info::Profile::BadgeType ConfirmInviteBox::BadgeForInvite(
		const ChatInvite &invite) {
	using Type = Info::Profile::BadgeType;
	return invite.isVerified
		? Type::Verified
		: invite.isScam
		? Type::Scam
		: invite.isFake
		? Type::Fake
		: Type::None;
}

void ConfirmInviteBox::prepare() {
	addButton(
		(_requestApprove
			? tr::lng_group_request_to_join()
			: _isChannel
			? tr::lng_profile_join_channel()
			: tr::lng_profile_join_group()),
		_submit);
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	while (_participants.size() > 4) {
		_participants.pop_back();
	}

	auto newHeight = st::confirmInviteStatusTop + _status->height() + st::boxPadding.bottom();
	if (!_participants.empty()) {
		int skip = (st::confirmInviteUsersWidth - 4 * st::confirmInviteUserPhotoSize) / 5;
		int padding = skip / 2;
		_userWidth = (st::confirmInviteUserPhotoSize + 2 * padding);
		int sumWidth = _participants.size() * _userWidth;
		int left = (st::boxWideWidth - sumWidth) / 2;
		for (const auto &participant : _participants) {
			auto name = new Ui::FlatLabel(this, st::confirmInviteUserName);
			name->resizeToWidth(st::confirmInviteUserPhotoSize + padding);
			name->setText(participant.user->firstName.isEmpty()
				? participant.user->name()
				: participant.user->firstName);
			name->moveToLeft(left + (padding / 2), st::confirmInviteUserNameTop);
			left += _userWidth;
		}

		newHeight += st::confirmInviteUserHeight;
	}
	if (_about) {
		const auto padding = st::confirmInviteAboutPadding;
		_about->resizeToWidth(st::boxWideWidth - padding.left() - padding.right());
		newHeight += padding.top() + _about->height() + padding.bottom();
	}
	if (_aboutRequests) {
		const auto padding = st::confirmInviteAboutRequestsPadding;
		_aboutRequests->resizeToWidth(st::boxWideWidth - padding.left() - padding.right());
		newHeight += padding.top() + _aboutRequests->height() + padding.bottom();
	}
	setDimensions(st::boxWideWidth, newHeight);
}

void ConfirmInviteBox::resizeEvent(QResizeEvent *e) {
	BoxContent::resizeEvent(e);

	const auto padding = st::boxRowPadding;
	auto nameWidth = width() - padding.left() - padding.right();
	auto badgeWidth = 0;
	if (const auto widget = _badge->widget()) {
		badgeWidth = st::infoVerifiedCheckPosition.x() + widget->width();
		nameWidth -= badgeWidth;
	}
	_title->resizeToWidth(std::min(nameWidth, _title->textMaxWidth()));
	_title->moveToLeft(
		(width() - _title->width() - badgeWidth) / 2,
		st::confirmInviteTitleTop);
	const auto badgeLeft = _title->x() + _title->width();
	const auto badgeTop = _title->y();
	const auto badgeBottom = _title->y() + _title->height();
	_badge->move(badgeLeft, badgeTop, badgeBottom);

	_status->move(
		(width() - _status->width()) / 2,
		st::confirmInviteStatusTop);
	auto bottom = _status->y()
		+ _status->height()
		+ st::boxPadding.bottom()
		+ (_participants.empty() ? 0 : st::confirmInviteUserHeight);
	if (_about) {
		const auto padding = st::confirmInviteAboutPadding;
		_about->move((width() - _about->width()) / 2, bottom + padding.top());
		bottom += padding.top() + _about->height() + padding.bottom();
	}
	if (_aboutRequests) {
		const auto padding = st::confirmInviteAboutRequestsPadding;
		_aboutRequests->move((width() - _aboutRequests->width()) / 2, bottom + padding.top());
	}
}

void ConfirmInviteBox::paintEvent(QPaintEvent *e) {
	BoxContent::paintEvent(e);

	Painter p(this);

	if (_photo) {
		if (const auto image = _photo->image(Data::PhotoSize::Small)) {
			const auto size = st::confirmInvitePhotoSize;
			p.drawPixmap(
				(width() - size) / 2,
				st::confirmInvitePhotoTop,
				image->pix(
					{ size, size },
					{ .options = Images::Option::RoundCircle }));
		}
	} else if (_photoEmpty) {
		_photoEmpty->paintCircle(
			p,
			(width() - st::confirmInvitePhotoSize) / 2,
			st::confirmInvitePhotoTop,
			width(),
			st::confirmInvitePhotoSize);
	}

	int sumWidth = _participants.size() * _userWidth;
	int left = (width() - sumWidth) / 2;
	for (auto &participant : _participants) {
		participant.user->paintUserpicLeft(
			p,
			participant.userpic,
			left + (_userWidth - st::confirmInviteUserPhotoSize) / 2,
			st::confirmInviteUserPhotoTop,
			width(),
			st::confirmInviteUserPhotoSize);
		left += _userWidth;
	}
}
