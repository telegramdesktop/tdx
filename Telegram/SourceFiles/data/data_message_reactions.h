/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "data/data_message_reaction_id.h"
#include "data/stickers/data_custom_emoji.h"

namespace Tdb {
class TLDupdateActiveEmojiReactions;
class TLDupdateAvailableMessageEffects;
class TLDupdateDefaultReactionType;
class TLDupdateSavedMessagesTags;
class TLsavedMessagesTags;
class TLemojiReaction;
class TLmessageEffect;
class TLunreadReaction;
class TLmessageReaction;
class TLDstickers;
class TLavailableReactions;
} // namespace Tdb

namespace Ui {
class AnimatedIcon;
} // namespace Ui

namespace Ui::Text {
class CustomEmoji;
} // namespace Ui::Text

namespace Data {

class SavedSublist;
class DocumentMedia;
class Session;

struct Reaction {
	ReactionId id;
	QString title;
	//not_null<DocumentData*> staticIcon;
	not_null<DocumentData*> appearAnimation;
	not_null<DocumentData*> selectAnimation;
	//not_null<DocumentData*> activateAnimation;
	//not_null<DocumentData*> activateEffects;
	DocumentData *centerIcon = nullptr;
	DocumentData *aroundAnimation = nullptr;
	int count = 0;
	bool active = false;
	bool effect = false;
	bool premium = false;
};

struct PossibleItemReactionsRef {
	std::vector<not_null<const Reaction*>> recent;
	std::vector<not_null<const Reaction*>> stickers;
	bool customAllowed = false;
	bool tags = false;
};

struct PossibleItemReactions {
	PossibleItemReactions() = default;
	explicit PossibleItemReactions(const PossibleItemReactionsRef &other);

	std::vector<Reaction> recent;
	std::vector<Reaction> stickers;
	bool customAllowed = false;
	bool tags = false;
};

[[nodiscard]] PossibleItemReactionsRef LookupPossibleReactions(
	not_null<HistoryItem*> item,
	bool paidInFront = false);

struct MyTagInfo {
	ReactionId id;
	QString title;
	int count = 0;
};

struct PaidReactionSend {
	int count = 0;
	bool valid = false;
	std::optional<bool> anonymous = false;
};

[[nodiscard]] PossibleItemReactionsRef ParsePossibleReactions(
	not_null<Main::Session*> session,
	const Tdb::TLavailableReactions &available);

class Reactions final : private CustomEmojiManager::Listener {
public:
	explicit Reactions(not_null<Session*> owner);
	~Reactions();

	[[nodiscard]] Session &owner() const {
		return *_owner;
	}
	[[nodiscard]] Main::Session &session() const;

#if 0 // mtp
	void refreshTop();
	void refreshRecent();
	void refreshRecentDelayed();
	void refreshDefault();
#endif
	void refreshMyTags(SavedSublist *sublist = nullptr);
	void refreshMyTagsDelayed();
#if 0 // mtp
	void refreshTags();
	void refreshEffects();
#endif
	void refreshActive(const Tdb::TLDupdateActiveEmojiReactions &data);
	void refreshEffects(const Tdb::TLDupdateAvailableMessageEffects &data);
	void refreshFavorite(const Tdb::TLDupdateDefaultReactionType &data);
	void refreshMyTags(const Tdb::TLDupdateSavedMessagesTags &data);

	enum class Type {
		Active,
		Recent,
		Top,
		All,
		MyTags,
		Tags,
		Effects,
	};
	[[nodiscard]] const std::vector<Reaction> &list(Type type) const;
	[[nodiscard]] const std::vector<MyTagInfo> &myTagsInfo() const;
	[[nodiscard]] const QString &myTagTitle(const ReactionId &id) const;
	[[nodiscard]] ReactionId favoriteId() const;
	[[nodiscard]] const Reaction *favorite() const;
	void setFavorite(const ReactionId &id);
	void incrementMyTag(const ReactionId &id, SavedSublist *sublist);
	void decrementMyTag(const ReactionId &id, SavedSublist *sublist);
	void renameTag(const ReactionId &id, const QString &name);
	[[nodiscard]] DocumentData *chooseGenericAnimation(
		not_null<DocumentData*> custom) const;
	[[nodiscard]] DocumentData *choosePaidReactionAnimation() const;

	[[nodiscard]] rpl::producer<> topUpdates() const;
	[[nodiscard]] rpl::producer<> recentUpdates() const;
	[[nodiscard]] rpl::producer<> defaultUpdates() const;
	[[nodiscard]] rpl::producer<> favoriteUpdates() const;
	[[nodiscard]] rpl::producer<> myTagsUpdates() const;
	[[nodiscard]] rpl::producer<> tagsUpdates() const;
	[[nodiscard]] rpl::producer<ReactionId> myTagRenamed() const;
	[[nodiscard]] rpl::producer<> effectsUpdates() const;

	void preloadReactionImageFor(const ReactionId &emoji);
	[[nodiscard]] QImage resolveReactionImageFor(const ReactionId &emoji);

	// This is used to reserve space for the effect in BottomInfo but not
	// actually paint anything, used in case we want to paint icon ourselves.
	static constexpr auto kFakeEffectId = EffectId(1);

	void preloadEffectImageFor(EffectId id);
	[[nodiscard]] QImage resolveEffectImageFor(EffectId id);

	void preloadAnimationsFor(const ReactionId &emoji);

#if 0 // mtp
	void send(not_null<HistoryItem*> item, bool addToRecent);
	[[nodiscard]] bool sending(not_null<HistoryItem*> item) const;

	void poll(not_null<HistoryItem*> item, crl::time now);

	void updateAllInHistory(not_null<PeerData*> peer, bool enabled);
#endif

	void clearTemporary();
	[[nodiscard]] Reaction *lookupTemporary(const ReactionId &id);
	[[nodiscard]] not_null<Reaction*> lookupPaid();
	[[nodiscard]] not_null<DocumentData*> paidToastAnimation();

	[[nodiscard]] rpl::producer<std::vector<Reaction>> myTagsValue(
		SavedSublist *sublist = nullptr);

	[[nodiscard]] bool isQuitPrevent();
	void schedulePaid(not_null<HistoryItem*> item);
	void undoScheduledPaid(not_null<HistoryItem*> item);
	[[nodiscard]] crl::time sendingScheduledPaidAt(
		not_null<HistoryItem*> item) const;
	[[nodiscard]] static crl::time ScheduledPaidDelay();

#if 0 // mtp
	[[nodiscard]] static bool HasUnread(const MTPMessageReactions &data);
	static void CheckUnknownForUnread(
		not_null<Session*> owner,
		const MTPMessage &message);
#endif

private:
	struct ImageSet {
		QImage image;
		std::shared_ptr<DocumentMedia> media;
		std::unique_ptr<Ui::AnimatedIcon> icon;
		bool fromSelectAnimation = false;
		bool effect = false;
	};
	struct TagsBySublist {
		TagsBySublist() = default;
		TagsBySublist(TagsBySublist&&) = default;
		TagsBySublist(const TagsBySublist&) = delete;
		TagsBySublist &operator=(TagsBySublist&&) = default;
		TagsBySublist &operator=(const TagsBySublist&) = delete;

		std::vector<Reaction> tags;
		std::vector<MyTagInfo> info;
		uint64 hash = 0;
		mtpRequestId requestId = 0;
		bool requestScheduled = false;
		bool updateScheduled = false;
	};
	struct EmojiResolved {
		QString emoji;
		mtpRequestId requestId = 0;
		bool resolved = false;
		bool active = false;
	};
	struct EffectResolved {
		uint64 id = 0;
		mtpRequestId requestId = 0;
		bool sticker = false;
		bool resolved = false;
	};

	[[nodiscard]] not_null<CustomEmojiManager::Listener*> resolveListener();
	void customEmojiResolveDone(not_null<DocumentData*> document) override;

#if 0 // mtp
	void requestTop();
	void requestRecent();
	void requestDefault();
#endif
	void requestGeneric();
	void requestMyTags(SavedSublist *sublist = nullptr);
#if 0 // mtp
	void requestTags();
	void requestEffects();

	void updateTop(const MTPDmessages_reactions &data);
	void updateRecent(const MTPDmessages_reactions &data);
	void updateDefault(const MTPDmessages_availableReactions &data);
	void updateGeneric(const MTPDmessages_stickerSet &data);
	void updateMyTags(
		SavedSublist *sublist,
		const MTPDmessages_savedReactionTags &data);
	void updateTags(const MTPDmessages_reactions &data);
	void updateEffects(const MTPDmessages_availableEffects &data);

	void recentUpdated();
#endif
	void defaultUpdated();
	void myTagsUpdated();
	void tagsUpdated();
	void effectsUpdated();
	void updateMyTags(
		SavedSublist *sublist,
		const Tdb::TLsavedMessagesTags &tags);

	[[nodiscard]] std::optional<Reaction> resolveById(const ReactionId &id);
	[[nodiscard]] std::vector<Reaction> resolveByIds(
		const std::vector<ReactionId> &ids,
		base::flat_set<ReactionId> &unresolved);
	[[nodiscard]] std::optional<Reaction> resolveByInfo(
		const MyTagInfo &info,
		SavedSublist *sublist);
	[[nodiscard]] std::vector<Reaction> resolveByInfos(
		const std::vector<MyTagInfo> &infos,
		base::flat_map<
			ReactionId,
			base::flat_set<SavedSublist*>> &unresolved,
		SavedSublist *sublist);
	void resolve(const ReactionId &id);
	void applyFavorite(const ReactionId &id);
	void scheduleMyTagsUpdate(SavedSublist *sublist);

#if 0 // mtp
	[[nodiscard]] std::optional<Reaction> parse(
		const MTPAvailableReaction &entry);
	[[nodiscard]] std::optional<Reaction> parse(
		const MTPAvailableEffect &entry);
#endif
	void updateFromData(const Tdb::TLDupdateActiveEmojiReactions &data);
	[[nodiscard]] std::optional<Reaction> parse(
		const Tdb::TLemojiReaction &entry);
	[[nodiscard]] std::optional<Reaction> parse(
		const Tdb::TLmessageEffect &entry);
	void updateGeneric(const Tdb::TLDstickers &data);
	void resolveEmojiNext();
	void resolveEmoji(const QString &emoji);
	void resolveEmoji(not_null<EmojiResolved*> entry);
	void checkAllActiveResolved();
	[[nodiscard]] bool allActiveResolved() const;

	void resolveEffectNext();
	void resolveEffect(uint64 id);
	void resolveEffect(not_null<EffectResolved*> entry);
	void checkAllEffectsResolved();
	[[nodiscard]] bool allEffectsResolved() const;

	void preloadEffect(const Reaction &effect);
	void preloadImageFor(const ReactionId &id);
	[[nodiscard]] QImage resolveImageFor(const ReactionId &id);
	void loadImage(
		ImageSet &set,
		not_null<DocumentData*> document,
		bool fromSelectAnimation);
	void generateImage(ImageSet &set, const QString &emoji);
	void setAnimatedIcon(ImageSet &set);
	void resolveReactionImages();
	void resolveEffectImages();
	void downloadTaskFinished();

	void fillPaidReactionAnimations() const;
	[[nodiscard]] DocumentData *randomLoadedFrom(
		std::vector<not_null<DocumentData*>> list) const;

#if 0 // mtp
	void repaintCollected();
	void pollCollected();
#endif

	void sendPaid();
	bool sendPaid(not_null<HistoryItem*> item);
	void sendPaidRequest(
		not_null<HistoryItem*> item,
		PaidReactionSend send);
	void sendPaidPrivacyRequest(
		not_null<HistoryItem*> item,
		PaidReactionSend send);
	void sendPaidFinish(
		not_null<HistoryItem*> item,
		PaidReactionSend send,
		bool success);
	void checkQuitPreventFinished();

	const not_null<Session*> _owner;

	std::vector<Reaction> _active;
	std::vector<Reaction> _available;
	std::vector<Reaction> _recent;
	std::vector<ReactionId> _recentIds;
	base::flat_set<ReactionId> _unresolvedRecent;
	base::flat_map<SavedSublist*, TagsBySublist> _myTags;
	base::flat_map<
		ReactionId,
		base::flat_set<SavedSublist*>> _unresolvedMyTags;
	std::vector<Reaction> _tags;
	std::vector<ReactionId> _tagsIds;
	base::flat_set<ReactionId> _unresolvedTags;
	std::vector<Reaction> _top;
	std::vector<ReactionId> _topIds;
	base::flat_set<ReactionId> _unresolvedTop;
	std::vector<not_null<DocumentData*>> _genericAnimations;
	mutable std::vector<not_null<DocumentData*>> _paidReactionAnimations;
	std::vector<Reaction> _effects;
	ReactionId _favoriteId;
	ReactionId _unresolvedFavoriteId;
	std::optional<Reaction> _favorite;
	base::flat_map<
		not_null<DocumentData*>,
		std::shared_ptr<DocumentMedia>> _iconsCache;
	base::flat_map<
		not_null<DocumentData*>,
		std::shared_ptr<DocumentMedia>> _genericCache;
	mutable base::flat_map<
		not_null<DocumentData*>,
		std::shared_ptr<DocumentMedia>> _paidReactionCache;
	rpl::event_stream<> _topUpdated;
	rpl::event_stream<> _recentUpdated;
	rpl::event_stream<> _defaultUpdated;
	rpl::event_stream<> _favoriteUpdated;
	rpl::event_stream<SavedSublist*> _myTagsUpdated;
	rpl::event_stream<> _tagsUpdated;
	rpl::event_stream<ReactionId> _myTagRenamed;
	rpl::event_stream<> _effectsUpdated;

	// We need &i->second stay valid while inserting new items.
	// So we use std::map instead of base::flat_map here.
	// Otherwise we could use flat_map<DocumentId, unique_ptr<Reaction>>.
	std::map<DocumentId, Reaction> _temporary;
	std::optional<Reaction> _paid;
	DocumentData *_paidToastAnimation = nullptr;

#if 0 // mtp
	base::Timer _topRefreshTimer;
	mtpRequestId _topRequestId = 0;
	uint64 _topHash = 0;

	mtpRequestId _recentRequestId = 0;
	bool _recentRequestScheduled = false;
	uint64 _recentHash = 0;

	mtpRequestId _defaultRequestId = 0;
	int32 _defaultHash = 0;
#endif
	std::vector<EmojiResolved> _emojiReactions;
	std::vector<EffectResolved> _resolveEffects;

	mtpRequestId _genericRequestId = 0;

	mtpRequestId _tagsRequestId = 0;
	uint64 _tagsHash = 0;

	mtpRequestId _effectsRequestId = 0;
	int32 _effectsHash = 0;

	base::flat_map<ReactionId, ImageSet> _images;
	rpl::lifetime _imagesLoadLifetime;
	bool _waitingForReactions = false;
	bool _waitingForEffects = false;

#if 0 // mtp
	base::flat_map<FullMsgId, mtpRequestId> _sentRequests;

	base::flat_map<not_null<HistoryItem*>, crl::time> _repaintItems;
	base::Timer _repaintTimer;
	base::flat_set<not_null<HistoryItem*>> _pollItems;
	base::flat_set<not_null<HistoryItem*>> _pollingItems;
	mtpRequestId _pollRequestId = 0;
#endif

	base::flat_map<not_null<HistoryItem*>, crl::time> _sendPaidItems;
	base::flat_map<not_null<HistoryItem*>, mtpRequestId> _sendingPaid;
	base::Timer _sendPaidTimer;

#if 0 // mtp
	mtpRequestId _saveFaveRequestId = 0;
#endif

	rpl::lifetime _lifetime;

};

struct RecentReaction {
	not_null<PeerData*> peer;
	bool unread = false;
	bool big = false;
	bool my = false;

	friend inline bool operator==(
		const RecentReaction &a,
		const RecentReaction &b) = default;
};

struct MessageReactionsTopPaid {
	PeerData *peer = nullptr;
	uint32 count : 30 = 0;
	uint32 top : 1 = 0;
	uint32 my : 1 = 0;

	friend inline bool operator==(
		const MessageReactionsTopPaid &a,
		const MessageReactionsTopPaid &b) = default;
};

class MessageReactions final {
public:
	explicit MessageReactions(not_null<HistoryItem*> item);
	~MessageReactions();

	using TopPaid = MessageReactionsTopPaid;

	void add(const ReactionId &id, bool addToRecent);
	void remove(const ReactionId &id);

#if 0 // mtp
	bool change(
		const QVector<MTPReactionCount> &list,
		const QVector<MTPMessagePeerReaction> &recent,
		const QVector<MTPMessageReactor> &top,
		bool min);
	[[nodiscard]] bool checkIfChanged(
		const QVector<MTPReactionCount> &list,
		const QVector<MTPMessagePeerReaction> &recent,
		bool min) const;
#endif
	bool change(const QVector<Tdb::TLmessageReaction> &list);
	bool change(const QVector<Tdb::TLunreadReaction> &list);

	[[nodiscard]] const std::vector<MessageReaction> &list() const;
	[[nodiscard]] auto recent() const
		-> const base::flat_map<ReactionId, std::vector<RecentReaction>> &;
	[[nodiscard]] const std::vector<TopPaid> &topPaid() const;
	[[nodiscard]] std::vector<ReactionId> chosen() const;
	[[nodiscard]] bool empty() const;

	[[nodiscard]] bool hasUnread() const;
	void markRead();

	void scheduleSendPaid(int count, std::optional<bool> anonymous);
	[[nodiscard]] int scheduledPaid() const;
	void cancelScheduledPaid();

	[[nodiscard]] PaidReactionSend startPaidSending();
	void finishPaidSending(PaidReactionSend send, bool success);

	[[nodiscard]] bool localPaidData() const;
	[[nodiscard]] int localPaidCount() const;
	[[nodiscard]] bool localPaidAnonymous() const;
	bool clearCloudData();

private:
	struct Paid {
		std::vector<TopPaid> top;
		uint32 scheduled: 29 = 0;
		uint32 scheduledFlag : 1 = 0;
		uint32 scheduledAnonymous : 1 = 0;
		uint32 scheduledPrivacySet : 1 = 0;
		uint32 sending : 29 = 0;
		uint32 sendingFlag : 1 = 0;
		uint32 sendingAnonymous : 1 = 0;
		uint32 sendingPrivacySet : 1 = 0;
	};
	const not_null<HistoryItem*> _item;

	std::vector<MessageReaction> _list;
	base::flat_map<ReactionId, std::vector<RecentReaction>> _recent;
	std::unique_ptr<Paid> _paid;

};

} // namespace Data
