/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "main/session/send_as_peers.h"

#include "data/data_user.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "data/data_changes.h"
#include "main/main_session.h"
#include "apiwrap.h"

#include "tdb/tdb_tl_scheme.h"

namespace Main {
namespace {

using namespace Tdb;

constexpr auto kRequestEach = 30 * crl::time(1000);

} // namespace

SendAsPeers::SendAsPeers(not_null<Session*> session)
: _session(session)
, _onlyMe({ { .peer = session->user(), .premiumRequired = false } }) {
	_session->changes().peerUpdates(
		Data::PeerUpdate::Flag::Rights
	) | rpl::map([=](const Data::PeerUpdate &update) {
		const auto peer = update.peer;
		const auto channel = peer->asChannel();
		return std::tuple(
			peer,
			peer->amAnonymous(),
			channel ? channel->isPublic() : false);
	}) | rpl::distinct_until_changed(
	) | rpl::filter([=](not_null<PeerData*> peer, bool, bool) {
		return _lists.contains(peer);
	}) | rpl::start_with_next([=](not_null<PeerData*> peer, bool, bool) {
		refresh(peer, true);
	}, _lifetime);
}

bool SendAsPeers::shouldChoose(not_null<PeerData*> peer) {
	refresh(peer);
	return Data::CanSendAnything(peer, false) && (list(peer).size() > 1);
}

void SendAsPeers::refresh(not_null<PeerData*> peer, bool force) {
	if (!peer->isMegagroup()) {
		return;
	}
	const auto now = crl::now();
	const auto i = _lastRequestTime.find(peer);
	const auto when = (i == end(_lastRequestTime)) ? -1 : i->second;
	if (!force && (when >= 0 && now < when + kRequestEach)) {
		return;
	}
	_lastRequestTime[peer] = now;
	request(peer);
}

const std::vector<SendAsPeer> &SendAsPeers::list(
		not_null<PeerData*> peer) const {
	const auto i = _lists.find(peer);
	return (i != end(_lists)) ? i->second : _onlyMe;
}

rpl::producer<not_null<PeerData*>> SendAsPeers::updated() const {
	return _updates.events();
}

void SendAsPeers::saveChosen(
		not_null<PeerData*> peer,
		not_null<PeerData*> chosen) {
#if 0 // mtp
	peer->session().api().request(MTPmessages_SaveDefaultSendAs(
		peer->input,
		chosen->input
	)).send();
#endif
	peer->session().sender().request(TLsetChatMessageSender(
		peerToTdbChat(peer->id),
		peerToSender(chosen->id)
	)).send();

	setChosen(peer, chosen->id);
}

void SendAsPeers::setChosen(not_null<PeerData*> peer, PeerId chosenId) {
	if (chosen(peer) == chosenId) {
		return;
	}
	const auto fallback = peer->amAnonymous()
		? peer
		: peer->session().user();
	if (!chosenId || fallback->id == chosenId) {
		_chosen.remove(peer);
	} else {
		_chosen[peer] = chosenId;
	}
	_updates.fire_copy(peer);
}

PeerId SendAsPeers::chosen(not_null<PeerData*> peer) const {
	const auto i = _chosen.find(peer);
	return (i != end(_chosen)) ? i->second : PeerId();
}

not_null<PeerData*> SendAsPeers::resolveChosen(
		not_null<PeerData*> peer) const {
	return ResolveChosen(peer, list(peer), chosen(peer));
}

not_null<PeerData*> SendAsPeers::ResolveChosen(
		not_null<PeerData*> peer,
		const std::vector<SendAsPeer> &list,
		PeerId chosen) {
	const auto i = ranges::find(list, chosen, [](const SendAsPeer &as) {
		return as.peer->id;
	});
	return (i != end(list))
		? i->peer
		: !list.empty()
		? list.front().peer
		: (peer->isMegagroup() && peer->amAnonymous())
		? peer
		: peer->session().user();
}

void SendAsPeers::request(not_null<PeerData*> peer) {
#if 0 // mtp
	peer->session().api().request(MTPchannels_GetSendAs(
		peer->input
	)).done([=](const MTPchannels_SendAsPeers &result) {
		auto parsed = std::vector<SendAsPeer>();
		auto &owner = peer->owner();
		result.match([&](const MTPDchannels_sendAsPeers &data) {
			owner.processUsers(data.vusers());
			owner.processChats(data.vchats());
			const auto &list = data.vpeers().v;
			parsed.reserve(list.size());
			for (const auto &as : list) {
				const auto &data = as.data();
				const auto peerId = peerFromMTP(data.vpeer());
				if (const auto peer = owner.peerLoaded(peerId)) {
					parsed.push_back({
						.peer = peer,
						.premiumRequired = data.is_premium_required(),
					});
				}
			}
		});
#endif
	peer->session().sender().request(TLgetChatAvailableMessageSenders(
		peerToTdbChat(peer->id)
	)).done([=](const TLDmessageSenders &result) {
		auto parsed = std::vector<SendAsPeer>();
		auto &owner = peer->owner();
		for (const auto &id : result.vsenders().v) {
			if (const auto peer = owner.peerLoaded(peerFromSender(id))) {
				parsed.push_back({
					.peer = peer,
					.premiumRequired = (peer->isBroadcast()
						&& !peer->isVerified()),
				});
			}
		}
		if (parsed.size() > 1) {
			auto &now = _lists[peer];
			if (now != parsed) {
				now = std::move(parsed);
				_updates.fire_copy(peer);
			}
		} else if (const auto i = _lists.find(peer); i != end(_lists)) {
			_lists.erase(i);
			_updates.fire_copy(peer);
		}
	}).send();
}

} // namespace Main
