/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"

namespace Tdb {
class TLDupdateGroupCall;
class TLDupdateGroupCallParticipant;
class TLDgroupCallParticipant;
class TLDgroupCall;
} // namespace Tdb

class PeerData;

class ApiWrap;

namespace Calls {
struct ParticipantVideoParams;
} // namespace Calls

namespace Data {

[[nodiscard]] const std::string &RtmpEndpointId();

struct LastSpokeTimes {
	crl::time anything = 0;
	crl::time voice = 0;
};

struct GroupCallParticipant {
	not_null<PeerData*> peer;
	std::shared_ptr<Calls::ParticipantVideoParams> videoParams;
#if 0 // goodToRemove
	TimeId date = 0;
	TimeId lastActive = 0;
	uint64 raisedHandRating = 0;
#endif
	uint32 ssrc = 0;
	int volume = 0;
	bool sounding : 1 = false;
	bool speaking : 1 = false;
	bool additionalSounding : 1 = false;
	bool additionalSpeaking : 1 = false;
	bool muted : 1 = false;
	bool mutedByMe : 1 = false;
	bool canSelfUnmute : 1 = false;
#if 0 // goodToRemove
	bool onlyMinLoaded : 1 = false;
	bool videoJoined = false;
	bool applyVolumeFromMin = true;
#endif
	bool canBeSpeaking = false;

	bool isHandRaised = false;
	bool canBeMutedForAllUsers = false;
	bool canBeUnmutedForAllUsers = false;
	bool canBeMutedForCurrentUser = false;
	bool canBeUnmutedForCurrentUser = false;
	QString order;

	[[nodiscard]] const std::string &cameraEndpoint() const;
	[[nodiscard]] const std::string &screenEndpoint() const;
	[[nodiscard]] bool cameraPaused() const;
	[[nodiscard]] bool screenPaused() const;

	[[nodiscard]] QString rowOrder() const;
};

class GroupCall final {
public:
	GroupCall(
		not_null<PeerData*> peer,
		CallId id,
#if 0 // goodToRemove
		CallId accessHash,
#endif
		TimeId scheduleDate,
		bool rtmp);
	~GroupCall();

	[[nodiscard]] CallId id() const;
#if 0 // mtp
	[[nodiscard]] bool loaded() const;
#endif
	[[nodiscard]] bool rtmp() const;
	[[nodiscard]] bool listenersHidden() const;
	[[nodiscard]] not_null<PeerData*> peer() const;
#if 0 // goodToRemove
	[[nodiscard]] MTPInputGroupCall input() const;
#endif
	[[nodiscard]] QString title() const {
		return _title.current();
	}
	[[nodiscard]] rpl::producer<QString> titleValue() const {
		return _title.value();
	}
	void setTitle(const QString &title) {
		_title = title;
	}
	[[nodiscard]] TimeId recordStartDate() const {
		return _recordStartDate.current();
	}
	[[nodiscard]] rpl::producer<TimeId> recordStartDateValue() const {
		return _recordStartDate.value();
	}
	[[nodiscard]] rpl::producer<TimeId> recordStartDateChanges() const {
		return _recordStartDate.changes();
	}
	[[nodiscard]] TimeId scheduleDate() const {
		return _scheduleDate.current();
	}
	[[nodiscard]] rpl::producer<TimeId> scheduleDateValue() const {
		return _scheduleDate.value();
	}
	[[nodiscard]] rpl::producer<TimeId> scheduleDateChanges() const {
		return _scheduleDate.changes();
	}
	[[nodiscard]] bool scheduleStartSubscribed() const {
		return _scheduleStartSubscribed.current();
	}
	[[nodiscard]] rpl::producer<bool> scheduleStartSubscribedValue() const {
		return _scheduleStartSubscribed.value();
	}
#if 0 // goodToRemove
	[[nodiscard]] int unmutedVideoLimit() const {
		return _unmutedVideoLimit.current();
	}
#endif
	[[nodiscard]] bool canEnableVideo() const {
		return _canEnableVideo;
	}
	[[nodiscard]] bool recordVideo() const {
		return _recordVideo.current();
	}

	struct RecentSpeaker {
		not_null<PeerData*> peer;
		bool speaking = false;
	};
	using RecentSpeakers = std::vector<RecentSpeaker>;
	[[nodiscard]] rpl::producer<> recentSpeakersUpdated() const;
	[[nodiscard]] const RecentSpeakers &recentSpeakers() const;

	void setPeer(not_null<PeerData*> peer);

	using Participant = GroupCallParticipant;
	struct ParticipantUpdate {
		std::optional<Participant> was;
		std::optional<Participant> now;
	};

	static constexpr auto kSoundStatusKeptFor = crl::time(1500);

	[[nodiscard]] auto participants() const
		-> const std::vector<Participant> &;
	void requestParticipants();
	[[nodiscard]] bool participantsLoaded() const;
	[[nodiscard]] PeerData *participantPeerByAudioSsrc(uint32 ssrc) const;
	[[nodiscard]] const Participant *participantByPeer(
		not_null<PeerData*> peer) const;
	[[nodiscard]] const Participant *participantByEndpoint(
		const std::string &endpoint) const;

	[[nodiscard]] rpl::producer<> participantsReloaded();
	[[nodiscard]] auto participantUpdated() const
		-> rpl::producer<ParticipantUpdate>;
	[[nodiscard]] auto participantSpeaking() const
		-> rpl::producer<not_null<Participant*>>;

#if 0 // goodToRemove
	void enqueueUpdate(const MTPUpdate &update);
	void applyLocalUpdate(
		const MTPDupdateGroupCallParticipants &update);
#endif

	void applyLocalVolume(
		not_null<PeerData*> participantPeer,
		bool mute,
		std::optional<int> volume);

	void applyUpdate(const Tdb::TLDupdateGroupCall &update);
	void applyUpdate(
		const Tdb::TLDupdateGroupCallParticipant &update,
		bool local = false);

	void applyLastSpoke(uint32 ssrc, LastSpokeTimes when, crl::time now);
	void applyActiveUpdate(
		PeerId participantPeerId,
		LastSpokeTimes when,
		PeerData *participantPeerLoaded);

	void resolveParticipants(const base::flat_set<uint32> &ssrcs);
	[[nodiscard]] rpl::producer<
		not_null<const base::flat_map<
			uint32,
			LastSpokeTimes>*>> participantsResolved() const {
		return _participantsResolved.events();
	}

	[[nodiscard]] int fullCount() const;
	[[nodiscard]] rpl::producer<int> fullCountValue() const;

	void setInCall();
	void reload();
	void reloadIfStale();
#if 0 // goodToRemove
	void processFullCall(const MTPphone_GroupCall &call);
#endif

	void setJoinMutedLocally(bool muted);
	[[nodiscard]] bool joinMuted() const;
	[[nodiscard]] bool canChangeJoinMuted() const;
	[[nodiscard]] bool joinedToTop() const;

private:
	enum class ApplySliceSource {
		FullReloaded,
		SliceLoaded,
		UnknownLoaded,
		UpdateReceived,
		UpdateConstructed,
	};
	enum class QueuedType : uint8 {
		VersionedParticipant,
		Participant,
		Call,
	};
	[[nodiscard]] ApiWrap &api() const;

#if 0 // goodToRemove
	void discard(const MTPDgroupCallDiscarded &data);
#endif
	[[nodiscard]] bool inCall() const;
#if 0 // goodToRemove
	void applyParticipantsSlice(
		const QVector<MTPGroupCallParticipant> &list,
		ApplySliceSource sliceSource);
#endif
	void applyParticipant(
		const Tdb::TLDgroupCallParticipant &data,
		ApplySliceSource sliceSource);
	void requestUnknownParticipants();
	void changePeerEmptyCallFlag();
	void checkFinishSpeakingByActive();
	void applyCallFields(const Tdb::TLDgroupCall &data);
#if 0 // goodToRemove
	void applyCallFields(const MTPDgroupCall &data);
	void applyEnqueuedUpdate(const MTPUpdate &update);
#endif
	void setServerParticipantsCount(int count);
	void computeParticipantsCount();
#if 0 // goodToRemove
	void processQueuedUpdates();
	void processFullCallUsersChats(const MTPphone_GroupCall &call);
	void processFullCallFields(const MTPphone_GroupCall &call);
	[[nodiscard]] bool requestParticipantsAfterReload(
		const MTPphone_GroupCall &call) const;
	[[nodiscard]] bool processSavedFullCall();
	void finishParticipantsSliceRequest();
#endif
	[[nodiscard]] Participant *findParticipant(not_null<PeerData*> peer);

	const CallId _id = 0;
#if 0 // goodToRemove
	const CallId _accessHash = 0;
#endif

	not_null<PeerData*> _peer;
	int _version = 0;
	mtpRequestId _participantsRequestId = 0;
	mtpRequestId _reloadRequestId = 0;
	crl::time _reloadLastFinished = 0;
	rpl::variable<QString> _title;

#if 0 // goodToRemove
	base::flat_multi_map<
		std::pair<int, QueuedType>,
		MTPUpdate> _queuedUpdates;
	base::Timer _reloadByQueuedUpdatesTimer;
	std::optional<MTPphone_GroupCall> _savedFull;
#endif

	std::vector<Participant> _participants;
	base::flat_map<uint32, not_null<PeerData*>> _participantPeerByAudioSsrc;
	base::flat_map<not_null<PeerData*>, crl::time> _speakingByActiveFinishes;
	base::Timer _speakingByActiveFinishTimer;
	QString _nextOffset;
	int _serverParticipantsCount = 0;
	rpl::variable<int> _fullCount = 0;
#if 0 // goodToRemove
	rpl::variable<int> _unmutedVideoLimit = 0;
#endif
	rpl::variable<bool> _recordVideo = 0;
	rpl::variable<TimeId> _recordStartDate = 0;
	rpl::variable<TimeId> _scheduleDate = 0;
	rpl::variable<bool> _scheduleStartSubscribed = false;
	bool _canEnableVideo = false;

	struct {
		rpl::event_stream<> updates;
		RecentSpeakers current;
	} _recentSpeakers;

	base::flat_map<uint32, LastSpokeTimes> _unknownSpokenSsrcs;
	base::flat_map<PeerId, LastSpokeTimes> _unknownSpokenPeerIds;
	rpl::event_stream<
		not_null<const base::flat_map<
			uint32,
			LastSpokeTimes>*>> _participantsResolved;
	mtpRequestId _unknownParticipantPeersRequestId = 0;

	rpl::event_stream<ParticipantUpdate> _participantUpdates;
	rpl::event_stream<not_null<Participant*>> _participantSpeaking;
	rpl::event_stream<> _participantsReloaded;

	bool _joinMuted = false;
	bool _canChangeJoinMuted = true;
	bool _allParticipantsLoaded = false;
#if 0 // goodToRemove
	bool _joinedToTop = false;
#endif
	bool _isJoined = false;
	bool _applyingQueuedUpdates = false;
	bool _rtmp = false;
	bool _listenersHidden = false;

};

} // namespace Data
