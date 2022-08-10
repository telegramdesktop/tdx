/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/pin_messages_box.h"

#include "apiwrap.h"
#include "data/data_chat.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/widgets/checkbox.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"

#include "tdb/tdb_sender.h"

namespace {

using namespace Tdb;

[[nodiscard]] bool IsOldForPin(
		MsgId id,
		not_null<PeerData*> peer,
		MsgId topicRootId) {
	const auto normal = peer->migrateToOrMe();
	const auto migrated = normal->migrateFrom();
	const auto top = Data::ResolveTopPinnedId(normal, topicRootId, migrated);
	if (!top) {
		return false;
	} else if (peer == migrated) {
		return peerIsChannel(top.peer) || (id < top.msg);
	} else if (migrated) {
		return peerIsChannel(top.peer) && (id < top.msg);
	} else {
		return (id < top.msg);
	}
}

} // namespace

void PinMessageBox(
		not_null<Ui::GenericBox*> box,
		not_null<HistoryItem*> item) {
	struct State {
		QPointer<Ui::Checkbox> pinForPeer;
		QPointer<Ui::Checkbox> notify;
		mtpRequestId requestId = 0;
	};

	const auto peer = item->history()->peer;
	const auto msgId = item->id;
	const auto topicRootId = item->topic() ? item->topicRootId() : MsgId();
	const auto pinningOld = IsOldForPin(msgId, peer, topicRootId);
	const auto state = box->lifetime().make_state<State>();
#if 0 // mtp
	const auto api = box->lifetime().make_state<MTP::Sender>(
		&peer->session().mtp());
#endif

	auto checkbox = [&]() -> object_ptr<Ui::Checkbox> {
		if (peer->isUser() && !peer->isSelf()) {
			auto object = object_ptr<Ui::Checkbox>(
				box,
				tr::lng_pinned_also_for_other(
					tr::now,
					lt_user,
					peer->shortName()),
				false,
				st::urlAuthCheckbox);
			object->setAllowTextLines();
			state->pinForPeer = Ui::MakeWeak(object.data());
			return object;
		} else if (!pinningOld && (peer->isChat() || peer->isMegagroup())) {
			auto object = object_ptr<Ui::Checkbox>(
				box,
				tr::lng_pinned_notify(tr::now),
				true,
				st::urlAuthCheckbox);
			object->setAllowTextLines();
			state->notify = Ui::MakeWeak(object.data());
			return object;
		}
		return { nullptr };
	}();

	auto pinMessage = [=] {
		if (state->requestId) {
			return;
		}

		const auto finished = crl::guard(box, [=] { box->closeBox(); });
		state->requestId = peer->session().sender().request(TLpinChatMessage(
			peerToTdbChat(peer->id),
			tl_int53(msgId.bare),
			tl_bool(state->notify && !state->notify->checked()),
			tl_bool(state->pinForPeer && !state->pinForPeer->checked())
		)).done(finished).fail(finished).send();
#if 0 // mtp
		auto flags = MTPmessages_UpdatePinnedMessage::Flags(0);
		if (state->notify && !state->notify->checked()) {
			flags |= MTPmessages_UpdatePinnedMessage::Flag::f_silent;
		}
		if (state->pinForPeer && !state->pinForPeer->checked()) {
			flags |= MTPmessages_UpdatePinnedMessage::Flag::f_pm_oneside;
		}
		state->requestId = api->request(MTPmessages_UpdatePinnedMessage(
			MTP_flags(flags),
			peer->input,
			MTP_int(msgId)
		)).done([=](const MTPUpdates &result) {
			peer->session().api().applyUpdates(result);
			box->closeBox();
		}).fail([=] {
			box->closeBox();
		}).send();
#endif
	};

	Ui::ConfirmBox(box, {
		.text = (pinningOld
			? tr::lng_pinned_pin_old_sure()
			: (peer->isChat() || peer->isMegagroup())
			? tr::lng_pinned_pin_sure_group()
			: tr::lng_pinned_pin_sure()),
		.confirmed = std::move(pinMessage),
		.confirmText = tr::lng_pinned_pin(),
	});

	if (checkbox) {
		auto padding = st::boxPadding;
		padding.setTop(padding.bottom());
		box->addRow(std::move(checkbox), std::move(padding));
	}
}
