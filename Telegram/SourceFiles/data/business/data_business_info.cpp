/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/business/data_business_info.h"

#include "apiwrap.h"
#include "base/unixtime.h"
#include "data/business/data_business_common.h"
#include "data/data_document.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"

#include "tdb/tdb_sender.h"
#include "tdb/tdb_tl_scheme.h"

namespace Data {
namespace {

using namespace Tdb;

#if 0 // mtp
[[nodiscard]] MTPBusinessWorkHours ToMTP(const WorkingHours &data) {
	const auto list = data.intervals.normalized().list;
	const auto proj = [](const WorkingInterval &data) {
		return MTPBusinessWeeklyOpen(MTP_businessWeeklyOpen(
			MTP_int(data.start / 60),
			MTP_int(data.end / 60)));
	};
	return MTP_businessWorkHours(
		MTP_flags(0),
		MTP_string(data.timezoneId),
		MTP_vector_from_range(list | ranges::views::transform(proj)));
}

[[nodiscard]] MTPBusinessAwayMessageSchedule ToMTP(
		const AwaySchedule &data) {
	Expects(data.type != AwayScheduleType::Never);

	return (data.type == AwayScheduleType::Always)
		? MTP_businessAwayMessageScheduleAlways()
		: (data.type == AwayScheduleType::OutsideWorkingHours)
		? MTP_businessAwayMessageScheduleOutsideWorkHours()
		: MTP_businessAwayMessageScheduleCustom(
			MTP_int(data.customInterval.start),
			MTP_int(data.customInterval.end));
}

[[nodiscard]] MTPInputBusinessAwayMessage ToMTP(const AwaySettings &data) {
	using Flag = MTPDinputBusinessAwayMessage::Flag;
	return MTP_inputBusinessAwayMessage(
		MTP_flags(data.offlineOnly ? Flag::f_offline_only : Flag()),
		MTP_int(data.shortcutId),
		ToMTP(data.schedule),
		ForMessagesToMTP(data.recipients));
}

[[nodiscard]] MTPInputBusinessGreetingMessage ToMTP(
		const GreetingSettings &data) {
	return MTP_inputBusinessGreetingMessage(
		MTP_int(data.shortcutId),
		ForMessagesToMTP(data.recipients),
		MTP_int(data.noActivityDays));
}
#endif

[[nodiscard]] std::optional<TLbusinessOpeningHours> ToTL(
		const WorkingHours &data) {
	if (!data) {
		return {};
	}
	const auto list = data.intervals.normalized().list;
	const auto proj = [](const WorkingInterval &data) {
		return MTPBusinessWeeklyOpen(MTP_businessWeeklyOpen(
			MTP_int(data.start / 60),
			MTP_int(data.end / 60)));
	};
	auto intervals = QVector<TLbusinessOpeningHoursInterval>();
	intervals.reserve(list.size());
	for (const auto &interval : list) {
		intervals.push_back(tl_businessOpeningHoursInterval(
			tl_int32(interval.start / 60),
			tl_int32(interval.end / 60)));
	}
	return tl_businessOpeningHours(
		tl_string(data.timezoneId),
		tl_vector<TLbusinessOpeningHoursInterval>(intervals));
}

[[nodiscard]] std::optional<TLbusinessGreetingMessageSettings> ToTL(
		const GreetingSettings &data) {
	if (!data) {
		return {};
	}
	return tl_businessGreetingMessageSettings(
		tl_int32(data.shortcutId),
		ForMessagesToTL(data.recipients),
		tl_int32(data.noActivityDays));
}
#if 0 // mtp
[[nodiscard]] MTPInputBusinessAwayMessage ToMTP(const AwaySettings &data) {
	using Flag = MTPDinputBusinessAwayMessage::Flag;
	return MTP_inputBusinessAwayMessage(
		MTP_flags(data.offlineOnly ? Flag::f_offline_only : Flag()),
		MTP_int(data.shortcutId),
		ToMTP(data.schedule),
		ForMessagesToMTP(data.recipients));
}

[[nodiscard]] MTPInputBusinessGreetingMessage ToMTP(
	const GreetingSettings &data) {
	return MTP_inputBusinessGreetingMessage(
		MTP_int(data.shortcutId),
		ForMessagesToMTP(data.recipients),
		MTP_int(data.noActivityDays));
}
#endif

[[nodiscard]] TLbusinessAwayMessageSchedule ToTL(const AwaySchedule &data) {
	Expects(data.type != AwayScheduleType::Never);

	return (data.type == AwayScheduleType::Always)
		? tl_businessAwayMessageScheduleAlways()
		: (data.type == AwayScheduleType::OutsideWorkingHours)
		? tl_businessAwayMessageScheduleOutsideOfOpeningHours()
		: tl_businessAwayMessageScheduleCustom(
			tl_int32(data.customInterval.start),
			tl_int32(data.customInterval.end));
}

[[nodiscard]] std::optional<TLbusinessAwayMessageSettings> ToTL(
		const AwaySettings &data) {
	if (!data) {
		return {};
	}
	return tl_businessAwayMessageSettings(
		tl_int32(data.shortcutId),
		ForMessagesToTL(data.recipients),
		ToTL(data.schedule),
		tl_bool(data.offlineOnly));
}

} // namespace

BusinessInfo::BusinessInfo(not_null<Session*> owner)
: _owner(owner) {
}

BusinessInfo::~BusinessInfo() = default;

void BusinessInfo::saveWorkingHours(
		WorkingHours data,
		Fn<void(QString)> fail) {
	const auto session = &_owner->session();
	auto details = session->user()->businessDetails();
	const auto &was = details.hours;
	if (was == data) {
		return;
	}

#if 0 // mtp
	using Flag = MTPaccount_UpdateBusinessWorkHours::Flag;
	session->api().request(MTPaccount_UpdateBusinessWorkHours(
		MTP_flags(data ? Flag::f_business_work_hours : Flag()),
		ToMTP(data)
	)).fail([=](const MTP::Error &error) {
#endif
	session->sender().request(TLsetBusinessOpeningHours(
		ToTL(data)
	)).fail([=](const Error &error) {
		auto details = session->user()->businessDetails();
		details.hours = was;
		session->user()->setBusinessDetails(std::move(details));
		if (fail) {
#if 0 // mtp
			fail(error.type());
#endif
			fail(error.message);
		}
	}).send();

	details.hours = std::move(data);
	session->user()->setBusinessDetails(std::move(details));
}

void BusinessInfo::saveChatIntro(ChatIntro data, Fn<void(QString)> fail) {
	const auto session = &_owner->session();
	auto details = session->user()->businessDetails();
	const auto &was = details.intro;
	if (was == data) {
		return;
	} else {
		const auto session = &_owner->session();
#if 0 // mtp
		using Flag = MTPaccount_UpdateBusinessIntro::Flag;
		session->api().request(MTPaccount_UpdateBusinessIntro(
			MTP_flags(data ? Flag::f_intro : Flag()),
			MTP_inputBusinessIntro(
				MTP_flags(data.sticker
					? MTPDinputBusinessIntro::Flag::f_sticker
					: MTPDinputBusinessIntro::Flag()),
				MTP_string(data.title),
				MTP_string(data.description),
				(data.sticker
					? data.sticker->mtpInput()
					: MTP_inputDocumentEmpty()))
		)).fail([=](const MTP::Error &error) {
#endif
		session->sender().request(TLsetBusinessStartPage(data
			? tl_inputBusinessStartPage(
				tl_string(data.title),
				tl_string(data.description),
				(data.sticker
					? tl_inputFileId(tl_int32(data.sticker->tdbFileId()))
					: std::optional<TLinputFile>()))
			: std::optional<TLinputBusinessStartPage>()
		)).fail([=](const Error &error) {
			auto details = session->user()->businessDetails();
			details.intro = was;
			session->user()->setBusinessDetails(std::move(details));
			if (fail) {
#if 0 // mtp
				fail(error.type());
#endif
				fail(error.message);
			}
		}).send();
	}

	details.intro = std::move(data);
	session->user()->setBusinessDetails(std::move(details));
}

void BusinessInfo::saveLocation(
		BusinessLocation data,
		Fn<void(QString)> fail) {
	const auto session = &_owner->session();
	auto details = session->user()->businessDetails();
	const auto &was = details.location;
	if (was == data) {
		return;
	} else {
		const auto session = &_owner->session();
		session->sender().request(TLsetBusinessLocation(
			((data.point || !data.address.isEmpty())
				? tl_businessLocation(
					(data.point
						? tl_location(
							tl_double(data.point->lat()),
							tl_double(data.point->lon()),
							tl_double(0))
						: std::optional<TLlocation>()),
					tl_string(data.address))
				: std::optional<TLbusinessLocation>())
		)).fail([=](const Error &error) {
#if 0 // mtp
		using Flag = MTPaccount_UpdateBusinessLocation::Flag;
		session->api().request(MTPaccount_UpdateBusinessLocation(
			MTP_flags((data.point ? Flag::f_geo_point : Flag())
				| (data.address.isEmpty() ? Flag() : Flag::f_address)),
			(data.point
				? MTP_inputGeoPoint(
					MTP_flags(0),
					MTP_double(data.point->lat()),
					MTP_double(data.point->lon()),
					MTPint()) // accuracy_radius
				: MTP_inputGeoPointEmpty()),
			MTP_string(data.address)
		)).fail([=](const MTP::Error &error) {
#endif
			auto details = session->user()->businessDetails();
			details.location = was;
			session->user()->setBusinessDetails(std::move(details));
			if (fail) {
#if 0 // mtp
				fail(error.type());
#endif
				fail(error.message);
			}
		}).send();
	}

	details.location = std::move(data);
	session->user()->setBusinessDetails(std::move(details));
}

void BusinessInfo::applyAwaySettings(AwaySettings data) {
	if (_awaySettings == data) {
		return;
	}
	_awaySettings = data;
	_awaySettingsChanged.fire({});
}

void BusinessInfo::saveAwaySettings(
		AwaySettings data,
		Fn<void(QString)> fail) {
	const auto &was = _awaySettings;
	if (was == data) {
		return;
	} else if (!data || data.shortcutId) {
#if 0 // mtp
		using Flag = MTPaccount_UpdateBusinessAwayMessage::Flag;
		const auto session = &_owner->session();
		session->api().request(MTPaccount_UpdateBusinessAwayMessage(
			MTP_flags(data ? Flag::f_message : Flag()),
			data ? ToMTP(data) : MTPInputBusinessAwayMessage()
		)).fail([=](const MTP::Error &error) {
#endif
		_owner->session().sender().request(TLsetBusinessAwayMessageSettings(
			ToTL(data)
		)).fail([=](const Error &error) {
			_awaySettings = was;
			_awaySettingsChanged.fire({});
			if (fail) {
#if 0 // mtp
				fail(error.type());
#endif
				fail(error.message);
			}
		}).send();
	}
	_awaySettings = std::move(data);
	_awaySettingsChanged.fire({});
}

bool BusinessInfo::awaySettingsLoaded() const {
	return _awaySettings.has_value();
}

AwaySettings BusinessInfo::awaySettings() const {
	return _awaySettings.value_or(AwaySettings());
}

rpl::producer<> BusinessInfo::awaySettingsChanged() const {
	return _awaySettingsChanged.events();
}

void BusinessInfo::applyGreetingSettings(GreetingSettings data) {
	if (_greetingSettings == data) {
		return;
	}
	_greetingSettings = data;
	_greetingSettingsChanged.fire({});
}

void BusinessInfo::saveGreetingSettings(
		GreetingSettings data,
		Fn<void(QString)> fail) {
	const auto &was = _greetingSettings;
	if (was == data) {
		return;
	} else if (!data || data.shortcutId) {
		_owner->session().sender().request(
			TLsetBusinessGreetingMessageSettings(ToTL(data))
		).fail([=](const Error &error) {
#if 0 // mtp
		using Flag = MTPaccount_UpdateBusinessGreetingMessage::Flag;
		_owner->session().api().request(
			MTPaccount_UpdateBusinessGreetingMessage(
				MTP_flags(data ? Flag::f_message : Flag()),
				data ? ToMTP(data) : MTPInputBusinessGreetingMessage())
		).fail([=](const MTP::Error &error) {
#endif
			_greetingSettings = was;
			_greetingSettingsChanged.fire({});
			if (fail) {
#if 0 // mtp
				fail(error.type());
#endif
				fail(error.message);
			}
		}).send();
	}
	_greetingSettings = std::move(data);
	_greetingSettingsChanged.fire({});
}

bool BusinessInfo::greetingSettingsLoaded() const {
	return _greetingSettings.has_value();
}

GreetingSettings BusinessInfo::greetingSettings() const {
	return _greetingSettings.value_or(GreetingSettings());
}

rpl::producer<> BusinessInfo::greetingSettingsChanged() const {
	return _greetingSettingsChanged.events();
}

void BusinessInfo::preload() {
	preloadTimezones();
}

void BusinessInfo::preloadTimezones() {
	if (!_timezones.current().list.empty() || _timezonesRequestId) {
		return;
	}
	_timezonesRequestId = _owner->session().sender().request(
		TLgetTimeZones()
	).done([=](const TLtimeZones &result) {
		const auto &data = result.data();
		{
			const auto proj = [](const TLtimeZone &result) {
#if 0 // mtp
	_timezonesRequestId = _owner->session().api().request(
		MTPhelp_GetTimezonesList(MTP_int(_timezonesHash))
	).done([=](const MTPhelp_TimezonesList &result) {
		result.match([&](const MTPDhelp_timezonesList &data) {
			_timezonesHash = data.vhash().v;
			const auto proj = [](const MTPtimezone &result) {
#endif
				return Timezone{
					.id = qs(result.data().vid()),
					.name = qs(result.data().vname()),
#if 0 // mtp
					.utcOffset = result.data().vutc_offset().v,
				};
			};
			_timezones = Timezones{
				.list = ranges::views::all(
					data.vtimezones().v
#endif
					.utcOffset = result.data().vutc_time_offset().v
				};
			};
			_timezones = Timezones{
				.list = ranges::views::all(
					data.vtime_zones().v
				) | ranges::views::transform(
					proj
				) | ranges::to_vector,
			};
#if 0 // mtp
		}, [](const MTPDhelp_timezonesListNotModified &) {
		});
#endif
		}
	}).send();
}

rpl::producer<Timezones> BusinessInfo::timezonesValue() const {
	const_cast<BusinessInfo*>(this)->preloadTimezones();
	return _timezones.value();
}

bool BusinessInfo::timezonesLoaded() const {
	return !_timezones.current().list.empty();
}

QString FindClosestTimezoneId(const std::vector<Timezone> &list) {
	const auto local = QDateTime::currentDateTime();
	const auto utc = QDateTime(local.date(), local.time(), Qt::UTC);
	const auto shift = base::unixtime::now() - (TimeId)::time(nullptr);
	const auto delta = int(utc.toSecsSinceEpoch())
		- int(local.toSecsSinceEpoch())
		- shift;
	const auto proj = [&](const Timezone &value) {
		auto distance = value.utcOffset - delta;
		while (distance > 12 * 3600) {
			distance -= 24 * 3600;
		}
		while (distance < -12 * 3600) {
			distance += 24 * 3600;
		}
		return std::abs(distance);
	};
	return ranges::min_element(list, ranges::less(), proj)->id;
}

} // namespace Data
