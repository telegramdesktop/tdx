/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_secret_chat.h"

#include "data/data_session.h"
#include "data/data_user.h"
#include "tdb/tdb_tl_scheme.h"

namespace {

using namespace Tdb;

} // namespace

SecretChatData::SecretChatData(not_null<Data::Session*> owner, PeerId id)
: PeerData(owner, id) {
}

bool SecretChatData::valid() const {
	return _user != nullptr;
}

not_null<UserData*> SecretChatData::user() const {
	Expects(valid());

	return _user;
}

void SecretChatData::update(const TLDsecretChat &data) {
	const auto outbound = data.vis_outbound().v;
	if (!_user) {
		_user = owner().user(data.vuser_id());
	} else {
		Assert(_user->id == peerFromUser(data.vuser_id()));
		Assert(out() == outbound);
	}

	const auto state = data.vstate().match([](
			const TLDsecretChatStatePending &) {
		return SecretChatDataFlag::Pending;
	}, [](const TLDsecretChatStateReady &) {
		return SecretChatDataFlag::Ready;
	}, [](const TLDsecretChatStateClosed &) {
		return SecretChatDataFlag::Closed;
	});
	_keyHash = data.vkey_hash().v;
	_layer = data.vlayer().v;
	const auto mask = Flag::Out
		| Flag::Pending
		| Flag::Ready
		| Flag::Closed;
	setFlags((flags() & ~mask) | (outbound ? Flag::Out : Flag()) | state);
}

SecretChatState SecretChatData::state() const {
	if (flags() & Flag::Pending) {
		return SecretChatState::Pending;
	} else if (flags() & Flag::Ready) {
		return SecretChatState::Ready;
	} else if (flags() & Flag::Closed) {
		return SecretChatState::Closed;
	}
	Unexpected("Flags in SecretChatData::state.");
}
