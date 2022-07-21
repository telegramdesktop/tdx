/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Tdb {
class TLchatNotificationSettings;
class TLscopeNotificationSettings;
} // namespace Tdb

namespace Data {

class NotifyPeerSettingsValue;

struct NotifySound {
	QString	title;
	QString data;
	DocumentId id = 0;
	bool none = false;
};

struct MuteValue {
	bool unmute = false;
	bool forever = false;
	int period = 0;

	[[nodiscard]] explicit operator bool() const {
		return unmute || forever || period;
	}
	[[nodiscard]] int until() const;
};

inline bool operator==(const NotifySound &a, const NotifySound &b) {
	return (a.id == b.id)
		&& (a.none == b.none)
		&& (a.title == b.title)
		&& (a.data == b.data);
}

class PeerNotifySettings {
public:
	PeerNotifySettings();

	[[nodiscard]] static Tdb::TLchatNotificationSettings Default();
	[[nodiscard]] static Tdb::TLscopeNotificationSettings ScopeDefault();

#if 0 // mtp
	bool change(const MTPPeerNotifySettings &settings);
#endif
	bool change(const Tdb::TLchatNotificationSettings &settings);
	bool change(
		MuteValue muteForSeconds,
		std::optional<bool> silentPosts,
		std::optional<NotifySound> sound,
		std::optional<bool> storiesMuted);
	bool resetToDefault();

	bool settingsUnknown() const;
	std::optional<TimeId> muteUntil() const;
	std::optional<bool> silentPosts() const;
	std::optional<NotifySound> sound() const;
#if 0 // mtp
	MTPinputPeerNotifySettings serialize() const;
#endif
	[[nodiscard]] Tdb::TLchatNotificationSettings serialize() const;
	[[nodiscard]] Tdb::TLscopeNotificationSettings serializeDefault() const;

	~PeerNotifySettings();

private:
	bool _known = false;
	std::unique_ptr<NotifyPeerSettingsValue> _value;

};

} // namespace Data
