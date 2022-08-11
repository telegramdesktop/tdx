/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/sender.h"

namespace Tdb {
class TLDupdateCall;
class TLDupdateNewCallSignalingData;
class TLDupdateGroupCall;
class TLDupdateGroupCallParticipant;
class TLDcall;
} // namespace Tdb

namespace crl {
class semaphore;
} // namespace crl

namespace Platform {
enum class PermissionType;
} // namespace Platform

namespace Media::Audio {
class Track;
} // namespace Media::Audio

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class Show;
} // namespace Ui

namespace Calls::Group {
struct JoinInfo;
class Panel;
class ChooseJoinAsProcess;
class StartRtmpProcess;
} // namespace Calls::Group

namespace tgcalls {
class VideoCaptureInterface;
} // namespace tgcalls

namespace Calls {

class Call;
enum class CallType;
class GroupCall;
class Panel;
struct DhConfig;

struct StartGroupCallArgs {
	enum class JoinConfirm {
		None,
		IfNowInAnother,
		Always,
	};
	QString joinHash;
	JoinConfirm confirm = JoinConfirm::IfNowInAnother;
	bool scheduleNeeded = false;
};

class Instance final : public base::has_weak_ptr {
public:
	Instance();
	~Instance();

	void startOutgoingCall(not_null<UserData*> user, bool video);
	void startOrJoinGroupCall(
		std::shared_ptr<Ui::Show> show,
		not_null<PeerData*> peer,
		StartGroupCallArgs args);
	void showStartWithRtmp(
		std::shared_ptr<Ui::Show> show,
		not_null<PeerData*> peer);

#if 0 // goodToRemove
	void handleUpdate(
		not_null<Main::Session*> session,
		const MTPUpdate &update);
#endif
	void handleUpdate(
		not_null<Main::Session*> session,
		const Tdb::TLDupdateCall &update);
	void handleUpdate(
		not_null<Main::Session*> session,
		const Tdb::TLDupdateNewCallSignalingData &update);
	void handleGroupCallUpdate(
		not_null<Main::Session*> session,
		const Tdb::TLDupdateGroupCall &update);
	void handleGroupCallUpdate(
		not_null<Main::Session*> session,
		const Tdb::TLDupdateGroupCallParticipant &update);

#if 0 // goodToRemove
	// Called by Data::GroupCall when it is appropriate by the 'version'.
	void applyGroupCallUpdateChecked(
		not_null<Main::Session*> session,
		const MTPUpdate &update);
#endif

	void showInfoPanel(not_null<Call*> call);
	void showInfoPanel(not_null<GroupCall*> call);
	[[nodiscard]] Call *currentCall() const;
	[[nodiscard]] rpl::producer<Call*> currentCallValue() const;
	[[nodiscard]] GroupCall *currentGroupCall() const;
	[[nodiscard]] rpl::producer<GroupCall*> currentGroupCallValue() const;
	[[nodiscard]] bool inCall() const;
	[[nodiscard]] bool inGroupCall() const;
	[[nodiscard]] bool hasVisiblePanel(
		Main::Session *session = nullptr) const;
	[[nodiscard]] bool hasActivePanel(
		Main::Session *session = nullptr) const;
	bool activateCurrentCall(const QString &joinHash = QString());
	bool minimizeCurrentActiveCall();
	bool toggleFullScreenCurrentActiveCall();
	bool closeCurrentActiveCall();
	[[nodiscard]] auto getVideoCapture(
		std::optional<QString> deviceId = std::nullopt,
		bool isScreenCapture = false)
		-> std::shared_ptr<tgcalls::VideoCaptureInterface>;
	void requestPermissionsOrFail(Fn<void()> onSuccess, bool video = true);

	[[nodiscard]] FnMut<void()> addAsyncWaiter();

	[[nodiscard]] bool isSharingScreen() const;
	[[nodiscard]] bool isQuitPrevent();

private:
	class Delegate;
	friend class Delegate;

	not_null<Media::Audio::Track*> ensureSoundLoaded(const QString &key);
	void playSoundOnce(const QString &key);

#if 0 // goodToRemove
	void createCall(not_null<UserData*> user, CallType type, bool isVideo);
#endif
	void createCall(
		not_null<UserData*> user,
		CallType type,
		int id,
		bool video);
	void destroyCall(not_null<Call*> call);
	void requestToCreateCall(not_null<UserData*> user, bool video);

#if 0 // goodToRemove
	void createGroupCall(
		Group::JoinInfo info,
		const MTPInputGroupCall &inputCall);
#endif
	void createGroupCall(Group::JoinInfo info, CallId id);
	void destroyGroupCall(not_null<GroupCall*> call);
	void confirmLeaveCurrent(
		std::shared_ptr<Ui::Show> show,
		not_null<PeerData*> peer,
		StartGroupCallArgs args,
		Fn<void(StartGroupCallArgs)> confirmed);

	void requestPermissionOrFail(
		Platform::PermissionType type,
		Fn<void()> onSuccess);

#if 0 // goodToRemove
	void refreshDhConfig();
	void refreshServerConfig(not_null<Main::Session*> session);
	bytes::const_span updateDhConfig(const MTPmessages_DhConfig &data);
#endif

	void destroyCurrentCall();

#if 0 // goodToRemove
	void handleCallUpdate(
		not_null<Main::Session*> session,
		const MTPPhoneCall &call);
#endif
	void handleCallUpdate(
		not_null<Main::Session*> session,
		const Tdb::TLDcall &call);
#if 0 // goodToRemove
	void handleSignalingData(
		not_null<Main::Session*> session,
		const MTPDupdatePhoneCallSignalingData &data);
	void handleGroupCallUpdate(
		not_null<Main::Session*> session,
		const MTPUpdate &update);
#endif

	const std::unique_ptr<Delegate> _delegate;
#if 0 // goodToRemove
	const std::unique_ptr<DhConfig> _cachedDhConfig;
#endif

#if 0 // goodToRemove
	crl::time _lastServerConfigUpdateTime = 0;
	base::weak_ptr<Main::Session> _serverConfigRequestSession;
#endif
	std::weak_ptr<tgcalls::VideoCaptureInterface> _videoCapture;

	std::unique_ptr<Call> _currentCall;
	rpl::event_stream<Call*> _currentCallChanges;
	std::unique_ptr<Panel> _currentCallPanel;

	std::unique_ptr<GroupCall> _currentGroupCall;
	rpl::event_stream<GroupCall*> _currentGroupCallChanges;
	std::unique_ptr<Group::Panel> _currentGroupCallPanel;

	base::flat_map<QString, std::unique_ptr<Media::Audio::Track>> _tracks;

	const std::unique_ptr<Group::ChooseJoinAsProcess> _chooseJoinAs;
	const std::unique_ptr<Group::StartRtmpProcess> _startWithRtmp;

	base::flat_set<std::unique_ptr<crl::semaphore>> _asyncWaiters;

};

} // namespace Calls
