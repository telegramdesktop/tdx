/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_peer_id.h"

#include "tdb/tdb_tl_scheme.h"

PeerId peerFromTdbChat(Tdb::TLint53 id) noexcept {
	//constexpr int64 MIN_SECRET_ID = -2002147483648ll; // From TDLib.
	//constexpr int64 ZERO_SECRET_ID = -2000000000000ll;
	//constexpr int64 MAX_SECRET_ID = -1997852516353ll;
	constexpr int64 MIN_CHANNEL_ID = -1997852516352ll;
	constexpr int64 MAX_CHANNEL_ID = -1000000000000ll;
	constexpr int64 MIN_CHAT_ID = -999999999999ll;
	//constexpr int64 MAX_USER_ID = 999999999999ll;
	if (id.v > 0) {
		return UserId(BareId(id.v));
	} else if (id.v < 0 && id.v > MIN_CHAT_ID) {
		return ChatId(BareId(-id.v));
	} else if (id.v < MAX_CHANNEL_ID && id.v > MIN_CHANNEL_ID) {
		return ChannelId(BareId(MAX_CHANNEL_ID - id.v));
	}
	return PeerId();
}

Tdb::TLint53 peerToTdbChat(PeerId id) noexcept {
	//constexpr int64 MIN_SECRET_ID = -2002147483648ll; // From TDLib.
	//constexpr int64 ZERO_SECRET_ID = -2000000000000ll;
	//constexpr int64 MAX_SECRET_ID = -1997852516353ll;
	//constexpr int64 MIN_CHANNEL_ID = -1997852516352ll;
	constexpr int64 MAX_CHANNEL_ID = -1000000000000ll;
	//constexpr int64 MIN_CHAT_ID = -999999999999ll;
	//constexpr int64 MAX_USER_ID = 999999999999ll;
	if (const auto userId = peerToUser(id)) {
		return Tdb::tl_int53(int64(userId.bare));
	} else if (const auto chatId = peerToChat(id)) {
		return Tdb::tl_int53(-int64(chatId.bare));
	} else if (const auto channelId = peerToChannel(id)) {
		return Tdb::tl_int53(MAX_CHANNEL_ID - int64(channelId.bare));
	}
	return Tdb::tl_int53(0);
}

PeerId peerFromSender(const Tdb::TLmessageSender &sender) noexcept {
	return sender.match([&](const Tdb::TLDmessageSenderUser &data) {
		return peerFromUser(data.vuser_id());
	}, [&](const Tdb::TLDmessageSenderChat &data) {
		return peerFromTdbChat(data.vchat_id());
	});
}

Tdb::TLmessageSender peerToSender(PeerId id) noexcept {
	if (const auto userId = peerToUser(id)) {
		return Tdb::tl_messageSenderUser(peerToTdbChat(id));
	} else {
		return Tdb::tl_messageSenderChat(peerToTdbChat(id));
	}
}

PeerId peerFromMTP(const MTPPeer &peer) {
	return peer.match([](const MTPDpeerUser &data) {
		return peerFromUser(data.vuser_id());
	}, [](const MTPDpeerChat &data) {
		return peerFromChat(data.vchat_id());
	}, [](const MTPDpeerChannel &data) {
		return peerFromChannel(data.vchannel_id());
	});
}

MTPpeer peerToMTP(PeerId id) {
	if (peerIsUser(id)) {
		return MTP_peerUser(peerToBareMTPInt(id));
	} else if (peerIsChat(id)) {
		return MTP_peerChat(peerToBareMTPInt(id));
	} else if (peerIsChannel(id)) {
		return MTP_peerChannel(peerToBareMTPInt(id));
	}
	return MTP_peerUser(MTP_long(0));
}

PeerId DeserializePeerId(quint64 serialized) {
	const auto flag = (UserId::kReservedBit << 48);
	const auto legacy = !(serialized & (UserId::kReservedBit << 48));
	if (!legacy) {
		return PeerId(serialized & (~flag));
	}
	constexpr auto PeerIdMask = uint64(0xFFFFFFFFULL);
	constexpr auto PeerIdTypeMask = uint64(0xF00000000ULL);
	constexpr auto PeerIdUserShift = uint64(0x000000000ULL);
	constexpr auto PeerIdChatShift = uint64(0x100000000ULL);
	constexpr auto PeerIdChannelShift = uint64(0x200000000ULL);
	constexpr auto PeerIdFakeShift = uint64(0xF00000000ULL);
	return ((serialized & PeerIdTypeMask) == PeerIdUserShift)
		? peerFromUser(UserId(serialized & PeerIdMask))
		: ((serialized & PeerIdTypeMask) == PeerIdChatShift)
		? peerFromChat(ChatId(serialized & PeerIdMask))
		: ((serialized & PeerIdTypeMask) == PeerIdChannelShift)
		? peerFromChannel(ChannelId(serialized & PeerIdMask))
		: ((serialized & PeerIdTypeMask) == PeerIdFakeShift)
		? PeerId(FakeChatId(serialized & PeerIdMask))
		: PeerId(0);
}

quint64 SerializePeerId(PeerId id) {
	Expects(!(id.value & (UserId::kReservedBit << 48)));

	return id.value | (UserId::kReservedBit << 48);
}
