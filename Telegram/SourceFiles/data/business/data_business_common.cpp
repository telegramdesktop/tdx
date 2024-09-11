/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/business/data_business_common.h"

#include "data/data_document.h"
#include "data/data_session.h"
#include "data/data_user.h"

#include "tdb/tdb_tl_scheme.h"

namespace Data {
namespace {

using namespace Tdb;

constexpr auto kDay = WorkingInterval::kDay;
constexpr auto kWeek = WorkingInterval::kWeek;
constexpr auto kInNextDayMax = WorkingInterval::kInNextDayMax;

[[nodiscard]] WorkingIntervals SortAndMerge(WorkingIntervals intervals) {
	auto &list = intervals.list;
	ranges::sort(list, ranges::less(), &WorkingInterval::start);
	for (auto i = 0, count = int(list.size()); i != count; ++i) {
		if (i && list[i] && list[i -1] && list[i].start <= list[i - 1].end) {
			list[i - 1] = list[i - 1].united(list[i]);
			list[i] = {};
		}
		if (!list[i]) {
			list.erase(list.begin() + i);
			--i;
			--count;
		}
	}
	return intervals;
}

[[nodiscard]] WorkingIntervals MoveTailToFront(WorkingIntervals intervals) {
	auto &list = intervals.list;
	auto after = WorkingInterval{ kWeek, kWeek + kDay };
	while (!list.empty()) {
		if (const auto tail = list.back().intersected(after)) {
			list.back().end = tail.start;
			if (!list.back()) {
				list.pop_back();
			}
			list.insert(begin(list), tail.shifted(-kWeek));
		} else {
			break;
		}
	}
	return intervals;
}

template <typename Flag>
auto RecipientsFlags(const BusinessRecipients &data) {
	const auto &chats = data.allButExcluded
		? data.excluded
		: data.included;
	using Type = BusinessChatType;
	return Flag()
		| ((chats.types & Type::NewChats) ? Flag::f_new_chats : Flag())
		| ((chats.types & Type::ExistingChats)
			? Flag::f_existing_chats
			: Flag())
		| ((chats.types & Type::Contacts) ? Flag::f_contacts : Flag())
		| ((chats.types & Type::NonContacts) ? Flag::f_non_contacts : Flag())
		| (chats.list.empty() ? Flag() : Flag::f_users)
		| (data.allButExcluded ? Flag::f_exclude_selected : Flag());
}

} // namespace

BusinessRecipients BusinessRecipients::MakeValid(BusinessRecipients value) {
	if (value.included.empty()) {
		value.allButExcluded = true;
	}
	return value;
}

#if 0 // mtp
MTPInputBusinessRecipients ForMessagesToMTP(const BusinessRecipients &data) {
	using Flag = MTPDinputBusinessRecipients::Flag;
	const auto &chats = data.allButExcluded ? data.excluded : data.included;
	return MTP_inputBusinessRecipients(
		MTP_flags(RecipientsFlags<Flag>(data)),
		MTP_vector_from_range(chats.list
			| ranges::views::transform(&UserData::inputUser)));
}

MTPInputBusinessBotRecipients ForBotsToMTP(const BusinessRecipients &data) {
	using Flag = MTPDinputBusinessBotRecipients::Flag;
	const auto &chats = data.allButExcluded ? data.excluded : data.included;
	return MTP_inputBusinessBotRecipients(
		MTP_flags(RecipientsFlags<Flag>(data)
			| ((data.allButExcluded || data.excluded.empty())
				? Flag()
				: Flag::f_exclude_users)),
		MTP_vector_from_range(chats.list
			| ranges::views::transform(&UserData::inputUser)),
		MTP_vector_from_range(data.excluded.list
			| ranges::views::transform(&UserData::inputUser)));
}

BusinessRecipients FromMTP(
		not_null<Session*> owner,
		const MTPBusinessRecipients &recipients) {
	using Type = BusinessChatType;

	const auto &data = recipients.data();
	auto result = BusinessRecipients{
		.allButExcluded = data.is_exclude_selected(),
	};
	auto &chats = result.allButExcluded
		? result.excluded
		: result.included;
	chats.types = Type()
		| (data.is_new_chats() ? Type::NewChats : Type())
		| (data.is_existing_chats() ? Type::ExistingChats : Type())
		| (data.is_contacts() ? Type::Contacts : Type())
		| (data.is_non_contacts() ? Type::NonContacts : Type());
	if (const auto users = data.vusers()) {
		for (const auto &userId : users->v) {
			chats.list.push_back(owner->user(UserId(userId.v)));
		}
	}
	return result;
}

BusinessRecipients FromMTP(
		not_null<Session*> owner,
		const MTPBusinessBotRecipients &recipients) {
	using Type = BusinessChatType;

	const auto &data = recipients.data();
	auto result = BusinessRecipients{
		.allButExcluded = data.is_exclude_selected(),
	};
	auto &chats = result.allButExcluded
		? result.excluded
		: result.included;
	chats.types = Type()
		| (data.is_new_chats() ? Type::NewChats : Type())
		| (data.is_existing_chats() ? Type::ExistingChats : Type())
		| (data.is_contacts() ? Type::Contacts : Type())
		| (data.is_non_contacts() ? Type::NonContacts : Type());
	if (const auto users = data.vusers()) {
		for (const auto &userId : users->v) {
			chats.list.push_back(owner->user(UserId(userId.v)));
		}
	}
	if (!result.allButExcluded) {
		if (const auto excluded = data.vexclude_users()) {
			for (const auto &userId : excluded->v) {
				result.excluded.list.push_back(
					owner->user(UserId(userId.v)));
			}
		}
	}
	return result;
}

BusinessDetails FromMTP(
		not_null<Session*> owner,
		const tl::conditional<MTPBusinessWorkHours> &hours,
		const tl::conditional<MTPBusinessLocation> &location,
		const tl::conditional<MTPBusinessIntro> &intro) {
	auto result = BusinessDetails();
	if (hours) {
		const auto &data = hours->data();
		result.hours.timezoneId = qs(data.vtimezone_id());
		result.hours.intervals.list = ranges::views::all(
			data.vweekly_open().v
		) | ranges::views::transform([](const MTPBusinessWeeklyOpen &open) {
			const auto &data = open.data();
			return WorkingInterval{
				data.vstart_minute().v * 60,
				data.vend_minute().v * 60,
			};
		}) | ranges::to_vector;
	}
	if (location) {
		const auto &data = location->data();
		result.location.address = qs(data.vaddress());
		if (const auto point = data.vgeo_point()) {
			point->match([&](const MTPDgeoPoint &data) {
				result.location.point = LocationPoint(data);
			}, [&](const MTPDgeoPointEmpty &) {
			});
		}
	}
	if (intro) {
		const auto &data = intro->data();
		result.intro.title = qs(data.vtitle());
		result.intro.description = qs(data.vdescription());
		if (const auto document = data.vsticker()) {
			result.intro.sticker = owner->processDocument(*document);
			if (!result.intro.sticker->sticker()) {
				result.intro.sticker = nullptr;
			}
		}
	}
	return result;
}

[[nodiscard]] AwaySettings FromMTP(
		not_null<Session*> owner,
		const tl::conditional<MTPBusinessAwayMessage> &message) {
	if (!message) {
		return AwaySettings();
	}
	const auto &data = message->data();
	auto result = AwaySettings{
		.recipients = FromMTP(owner, data.vrecipients()),
		.shortcutId = data.vshortcut_id().v,
		.offlineOnly = data.is_offline_only(),
	};
	data.vschedule().match([&](
			const MTPDbusinessAwayMessageScheduleAlways &) {
		result.schedule.type = AwayScheduleType::Always;
	}, [&](const MTPDbusinessAwayMessageScheduleOutsideWorkHours &) {
		result.schedule.type = AwayScheduleType::OutsideWorkingHours;
	}, [&](const MTPDbusinessAwayMessageScheduleCustom &data) {
		result.schedule.type = AwayScheduleType::Custom;
		result.schedule.customInterval = WorkingInterval{
			data.vstart_date().v,
			data.vend_date().v,
		};
	});
	return result;
}

[[nodiscard]] GreetingSettings FromMTP(
		not_null<Session*> owner,
		const tl::conditional<MTPBusinessGreetingMessage> &message) {
	if (!message) {
		return GreetingSettings();
	}
	const auto &data = message->data();
	return GreetingSettings{
		.recipients = FromMTP(owner, data.vrecipients()),
		.noActivityDays = data.vno_activity_days().v,
		.shortcutId = data.vshortcut_id().v,
	};
}
#endif

TLbusinessRecipients ForMessagesToTL(const BusinessRecipients &data) {
	auto copy = data;
	(copy.allButExcluded ? copy.included : copy.excluded) = {};
	return ForBotsToTL(copy);
}

TLbusinessRecipients ForBotsToTL(const BusinessRecipients &data) {
	const auto &chats = data.allButExcluded ? data.excluded : data.included;
	auto chatIds = QVector<TLint53>();
	chatIds.reserve(chats.list.size());
	for (const auto &user : chats.list) {
		chatIds.push_back(peerToTdbChat(user->id));
	}
	auto excludedChatIds = QVector<TLint53>();
	if (!data.allButExcluded) {
		excludedChatIds.reserve(data.excluded.list.size());
		for (const auto &user : data.excluded.list) {
			excludedChatIds.push_back(peerToTdbChat(user->id));
		}
	}
	using Type = BusinessChatType;
	return tl_businessRecipients(
		tl_vector<TLint53>(chatIds),
		tl_vector<TLint53>(excludedChatIds),
		tl_bool(chats.types & Type::ExistingChats),
		tl_bool(chats.types & Type::NewChats),
		tl_bool(chats.types & Type::Contacts),
		tl_bool(chats.types & Type::NonContacts),
		tl_bool(data.allButExcluded));
}

BusinessRecipients FromTL(
		not_null<Session*> owner,
		const TLbusinessRecipients &recipients) {
	using Type = BusinessChatType;

	const auto &data = recipients.data();
	auto result = BusinessRecipients{
		.allButExcluded = data.vexclude_selected().v,
	};

	auto &chats = result.allButExcluded
		? result.excluded
		: result.included;
	chats.types = Type()
		| (data.vselect_new_chats().v ? Type::NewChats : Type())
		| (data.vselect_existing_chats().v ? Type::ExistingChats : Type())
		| (data.vselect_contacts().v ? Type::Contacts : Type())
		| (data.vselect_non_contacts().v ? Type::NonContacts : Type());
	for (const auto chatId : data.vchat_ids().v) {
		if (const auto userId = peerToUser(peerFromTdbChat(chatId))) {
			chats.list.push_back(owner->user(userId));
		}
	}
	for (const auto chatId : data.vexcluded_chat_ids().v) {
		if (const auto userId = peerToUser(peerFromTdbChat(chatId))) {
			result.excluded.list.push_back(owner->user(userId));
		}
	}
	return result;
}

BusinessDetails FromTL(
		not_null<Session*> owner,
		const tl::conditional<TLbusinessInfo> &info) {
	if (!info) {
		return {};
	}
	auto result = BusinessDetails();
	const auto &data = info->data();
	if (const auto hours = data.vopening_hours()) {
		const auto &data = hours->data();
		result.hours.timezoneId = data.vtime_zone_id().v;
		result.hours.intervals.list = ranges::views::all(
			data.vopening_hours().v
		) | ranges::views::transform([](
				const TLbusinessOpeningHoursInterval &open) {
			const auto &data = open.data();
			return WorkingInterval{
				data.vstart_minute().v * 60,
				data.vend_minute().v * 60,
			};
		}) | ranges::to_vector;
	}
	if (const auto location = data.vlocation()) {
		const auto &data = location->data();
		result.location.address = data.vaddress().v;
		if (const auto point = data.vlocation()) {
			result.location.point = LocationPoint(*point);
		}
	}
	if (const auto intro = data.vstart_page()) {
		const auto &data = intro->data();
		result.intro.title = data.vtitle().v;
		result.intro.description = data.vmessage().v;
		if (const auto document = data.vsticker()) {
			result.intro.sticker = owner->processDocument(*document);
			if (!result.intro.sticker->sticker()) {
				result.intro.sticker = nullptr;
			}
		}
	}
	return result;
}

AwaySettings FromTL(
		not_null<Session*> owner,
		const tl::conditional<TLbusinessAwayMessageSettings> &settings) {
	if (!settings) {
		return AwaySettings();
	}
	const auto &data = settings->data();
	auto result = AwaySettings{
		.recipients = FromTL(owner, data.vrecipients()),
		.shortcutId = data.vshortcut_id().v,
		.offlineOnly = data.voffline_only().v,
	};
	data.vschedule().match([&](
			const TLDbusinessAwayMessageScheduleAlways &) {
		result.schedule.type = AwayScheduleType::Always;
	}, [&](const TLDbusinessAwayMessageScheduleOutsideOfOpeningHours &) {
		result.schedule.type = AwayScheduleType::OutsideWorkingHours;
	}, [&](const TLDbusinessAwayMessageScheduleCustom &data) {
		result.schedule.type = AwayScheduleType::Custom;
		result.schedule.customInterval = WorkingInterval{
			data.vstart_date().v,
			data.vend_date().v,
		};
	});
	return result;
}

[[nodiscard]] GreetingSettings FromTL(
		not_null<Session*> owner,
		const tl::conditional<TLbusinessGreetingMessageSettings> &settings) {
	if (!settings) {
		return GreetingSettings();
	}
	const auto &data = settings->data();
	return GreetingSettings{
		.recipients = FromTL(owner, data.vrecipients()),
		.noActivityDays = data.vinactivity_days().v,
		.shortcutId = data.vshortcut_id().v,
	};
}

WorkingIntervals WorkingIntervals::normalized() const {
	return SortAndMerge(MoveTailToFront(SortAndMerge(*this)));
}

WorkingIntervals ExtractDayIntervals(
		const WorkingIntervals &intervals,
		int dayIndex) {
	Expects(dayIndex >= 0 && dayIndex < 7);

	auto result = WorkingIntervals();
	auto &list = result.list;
	for (const auto &interval : intervals.list) {
		const auto now = interval.intersected(
			{ (dayIndex - 1) * kDay, (dayIndex + 2) * kDay });
		const auto after = interval.intersected(
			{ (dayIndex + 6) * kDay, (dayIndex + 9) * kDay });
		const auto before = interval.intersected(
			{ (dayIndex - 8) * kDay, (dayIndex - 5) * kDay });
		if (now) {
			list.push_back(now.shifted(-dayIndex * kDay));
		}
		if (after) {
			list.push_back(after.shifted(-(dayIndex + 7) * kDay));
		}
		if (before) {
			list.push_back(before.shifted(-(dayIndex - 7) * kDay));
		}
	}
	result = result.normalized();

	const auto outside = [&](WorkingInterval interval) {
		return (interval.end <= 0) || (interval.start >= kDay);
	};
	list.erase(ranges::remove_if(list, outside), end(list));

	if (!list.empty() && list.back().start <= 0 && list.back().end >= kDay) {
		list.back() = { 0, kDay };
	} else if (!list.empty() && (list.back().end > kDay + kInNextDayMax)) {
		list.back() = list.back().intersected({ 0, kDay });
	}
	if (!list.empty() && list.front().start <= 0) {
		if (list.front().start < 0
			&& list.front().end <= kInNextDayMax
			&& list.front().start > -kDay) {
			list.erase(begin(list));
		} else {
			list.front() = list.front().intersected({ 0, kDay });
			if (!list.front()) {
				list.erase(begin(list));
			}
		}
	}

	return result;
}

bool IsFullOpen(const WorkingIntervals &extractedDay) {
	return extractedDay // 00:00-23:59 or 00:00-00:00 (next day)
		&& (extractedDay.list.front() == WorkingInterval{ 0, kDay - 60 }
			|| extractedDay.list.front() == WorkingInterval{ 0, kDay });
}

WorkingIntervals RemoveDayIntervals(
		const WorkingIntervals &intervals,
		int dayIndex) {
	auto result = intervals.normalized();
	auto &list = result.list;
	const auto day = WorkingInterval{ 0, kDay };
	const auto shifted = day.shifted(dayIndex * kDay);
	auto before = WorkingInterval{ 0, shifted.start };
	auto after = WorkingInterval{ shifted.end, kWeek };
	for (auto i = 0, count = int(list.size()); i != count; ++i) {
		if (list[i].end <= shifted.start || list[i].start >= shifted.end) {
			continue;
		} else if (list[i].end <= shifted.start + kInNextDayMax
			&& (list[i].start < shifted.start
				|| (!dayIndex // This 'Sunday' finishing on next day <= 6:00.
					&& list[i].start == shifted.start
					&& list.back().end >= kWeek))) {
			continue;
		} else if (const auto first = list[i].intersected(before)) {
			list[i] = first;
			if (const auto second = list[i].intersected(after)) {
				list.push_back(second);
			}
		} else if (const auto second = list[i].intersected(after)) {
			list[i] = second;
		} else {
			list.erase(list.begin() + i);
			--i;
			--count;
		}
	}
	return result.normalized();
}

WorkingIntervals ReplaceDayIntervals(
		const WorkingIntervals &intervals,
		int dayIndex,
		WorkingIntervals replacement) {
	auto result = RemoveDayIntervals(intervals, dayIndex);
	const auto first = result.list.insert(
		end(result.list),
		begin(replacement.list),
		end(replacement.list));
	for (auto &interval : ranges::make_subrange(first, end(result.list))) {
		interval = interval.shifted(dayIndex * kDay);
	}
	return result.normalized();
}

} // namespace Data
