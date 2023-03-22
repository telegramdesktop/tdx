/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/flags.h"
#include "data/data_flags.h"
#include "data/data_peer.h"

namespace Tdb {
class TLDsecretChat;
} // namespace Tdb

enum class SecretChatDataFlag {
	Out = (1 << 0),
	Pending = (1 << 2),
	Ready = (1 << 3),
	Closed = (1 << 4),
};
inline constexpr bool is_flag_type(SecretChatDataFlag) { return true; };
using SecretChatDataFlags = base::flags<SecretChatDataFlag>;

enum class SecretChatState : uchar {
	Pending,
	Ready,
	Closed,
};

class SecretChatData final : public PeerData {
public:
	using Flag = SecretChatDataFlag;
	using Flags = Data::Flags<SecretChatDataFlags>;

	SecretChatData(not_null<Data::Session*> owner, PeerId id);

	[[nodiscard]] bool valid() const;
	[[nodiscard]] not_null<UserData*> user() const;

	void setFlags(SecretChatDataFlags which) {
		_flags.set(which);
	}
	void addFlags(SecretChatDataFlags which) {
		_flags.add(which);
	}
	void removeFlags(SecretChatDataFlags which) {
		_flags.remove(which);
	}
	[[nodiscard]] auto flags() const {
		return _flags.current();
	}
	[[nodiscard]] auto flagsValue() const {
		return _flags.value();
	}

	[[nodiscard]] bool out() const {
		return _flags.current() & Flag::Out;
	}
	[[nodiscard]] int layer() const {
		return _layer;
	}
	[[nodiscard]] QByteArray keyHash() const {
		return _keyHash.current();
	}
	[[nodiscard]] rpl::producer<QByteArray> keyHashValue() const {
		return _keyHash.value();
	}
	[[nodiscard]] SecretChatState state() const;

	void update(const Tdb::TLDsecretChat &data);

private:
	UserData *_user = nullptr;
	rpl::variable<QByteArray> _keyHash;
	Flags _flags;
	int _layer = 0;

};