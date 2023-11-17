/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/object_ptr.h"

namespace Tdb {
class TLchatBoostStatus;
class TLchatBoostSlots;
} // namespace Tdb

class ChannelData;

namespace Main {
class Session;
} // namespace Main

namespace Ui {
struct BoostCounters;
struct BoostFeatures;
class BoxContent;
class RpWidget;
} // namespace Ui

struct TakenBoostSlot {
	int id = 0;
	TimeId expires = 0;
	PeerId peerId = 0;
	TimeId cooldown = 0;
};

struct ForChannelBoostSlots {
	std::vector<int> free;
	std::vector<int> already;
	std::vector<TakenBoostSlot> other;
};

#if 0 // mtp
[[nodiscard]] ForChannelBoostSlots ParseForChannelBoostSlots(
	not_null<ChannelData*> channel,
	const QVector<MTPMyBoost> &boosts);

[[nodiscard]] Ui::BoostCounters ParseBoostCounters(
	const MTPpremium_BoostsStatus &status);
#endif

[[nodiscard]] ForChannelBoostSlots ParseForChannelBoostSlots(
	not_null<ChannelData*> channel,
	const Tdb::TLchatBoostSlots &slots);

[[nodiscard]] Ui::BoostCounters ParseBoostCounters(
	const Tdb::TLchatBoostStatus &status);

[[nodiscard]] Ui::BoostFeatures LookupBoostFeatures(
	not_null<ChannelData*> channel);

[[nodiscard]] int BoostsForGift(not_null<Main::Session*> session);

object_ptr<Ui::BoxContent> ReassignBoostsBox(
	not_null<ChannelData*> to,
	std::vector<TakenBoostSlot> from,
	Fn<void(std::vector<int> slots, int groups, int channels)> reassign,
	Fn<void()> cancel);

[[nodiscard]] object_ptr<Ui::RpWidget> CreateBoostReplaceUserpics(
	not_null<Ui::RpWidget*> parent,
	rpl::producer<std::vector<not_null<PeerData*>>> from,
	not_null<PeerData*> to);

[[nodiscard]] object_ptr<Ui::RpWidget> CreateUserpicsWithMoreBadge(
	not_null<Ui::RpWidget*> parent,
	rpl::producer<std::vector<not_null<PeerData*>>> peers,
	int limit);
