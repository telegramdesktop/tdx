/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/mtproto_auth_key.h"
#include "mtproto/mtp_instance.h"
#include "base/weak_ptr.h"

namespace Tdb {
class TLuser;
class Account;
class Sender;
class Options;
} // namespace Tdb

namespace Storage {
class Account;
class Domain;
enum class StartResult : uchar;
} // namespace Storage

namespace MTP {
class AuthKey;
class Config;
} // namespace MTP

namespace Main {

class Domain;
class Session;
class SessionSettings;
class AppConfig;

class Account final : public base::has_weak_ptr {
public:
	Account(not_null<Domain*> domain, const QString &dataName, int index);
	~Account();

	[[nodiscard]] Domain &domain() const {
		return *_domain;
	}

	[[nodiscard]] Storage::Domain &domainLocal() const;

	[[nodiscard]] Storage::StartResult legacyStart(
		const QByteArray &passcode);
	[[nodiscard]] std::unique_ptr<MTP::Config> prepareToStart(
		std::shared_ptr<MTP::AuthKey> localKey);
	void prepareToStartAdded(
		std::shared_ptr<MTP::AuthKey> localKey);
	void start(std::unique_ptr<MTP::Config> config);

	[[nodiscard]] uint64 willHaveSessionUniqueId(MTP::Config *config) const;
#if 0 // mtp
	void createSession(
		const MTPUser &user,
		std::unique_ptr<SessionSettings> settings = nullptr);
#endif
	void createSession(
		UserId id,
		QByteArray serialized,
		int streamVersion,
		std::unique_ptr<SessionSettings> settings);

	void createSession(
		const Tdb::TLuser &user,
		std::unique_ptr<SessionSettings> settings = nullptr);

	void logOut();
	void forcedLogOut();
	[[nodiscard]] bool loggingOut() const;

	[[nodiscard]] AppConfig &appConfig() const {
		Expects(_appConfig != nullptr);

		return *_appConfig;
	}

	[[nodiscard]] Storage::Account &local() const {
		return *_local;
	}

	[[nodiscard]] bool sessionExists() const;
	[[nodiscard]] Session &session() const;
	[[nodiscard]] Session *maybeSession() const;
	[[nodiscard]] rpl::producer<Session*> sessionValue() const;
	[[nodiscard]] rpl::producer<Session*> sessionChanges() const;

	[[nodiscard]] Tdb::Account &tdb() const {
		Expects(_tdb != nullptr);

		return *_tdb;
	}
	[[nodiscard]] Tdb::Sender &sender() const;
	[[nodiscard]] Tdb::Options &options() const;
	[[nodiscard]] QString internalLinksDomain() const;
	[[nodiscard]] bool testMode() const;

	[[nodiscard]] MTP::Instance &mtp() const {
		return *_mtp;
	}
#if 0 // mtp
	[[nodiscard]] rpl::producer<not_null<MTP::Instance*>> mtpValue() const;

	// Each time the main session changes a new copy of the pointer is fired.
	// This allows to resend the requests that were not requiring auth, and
	// which could be forgotten without calling .done() or .fail() because
	// of the main dc changing.
	[[nodiscard]] auto mtpMainSessionValue() const
		-> rpl::producer<not_null<MTP::Instance*>>;
#endif

	// Set from legacy storage.
	void setLegacyMtpKey(std::shared_ptr<MTP::AuthKey> key);

	void setMtpMainDcId(MTP::DcId mainDcId);
	void setSessionUserId(UserId userId);
	void setSessionFromStorage(
		std::unique_ptr<SessionSettings> data,
		QByteArray &&selfSerialized,
		int32 selfStreamVersion);
	[[nodiscard]] SessionSettings *getSessionSettings();
	[[nodiscard]] rpl::producer<> mtpNewSessionCreated() const;
	[[nodiscard]] rpl::producer<MTPUpdates> mtpUpdates() const;

	// Serialization.
	[[nodiscard]] QByteArray serializeMtpAuthorization() const;
	void setMtpAuthorization(const QByteArray &serialized);

#if 0 // mtp
	void suggestMainDcId(MTP::DcId mainDcId);
	void destroyStaleAuthorizationKeys();
#endif
	enum class ConnectionState {
		WaitingForNetwork,
		ConnectingToProxy,
		Connecting,
		Updating,
		Ready,
	};
	void setConnectionState(ConnectionState state);
	[[nodiscard]] ConnectionState connectionState() const;

	void setHandleLoginCode(Fn<void(QString)> callback);
	void handleLoginCode(const QString &code) const;

	[[nodiscard]] rpl::lifetime &lifetime() {
		return _lifetime;
	}

private:
	static constexpr auto kDefaultSaveDelay = crl::time(1000);
	enum class DestroyReason {
		Quitting,
		LoggedOut,
	};

	void startMtp(std::unique_ptr<MTP::Config> config);
#if 0 // mtp
	void createSession(
		const MTPUser &user,
		QByteArray serialized,
		int streamVersion,
		std::unique_ptr<SessionSettings> settings);
#endif
	void createSession(
		const Tdb::TLuser &user,
		QByteArray serialized,
		int streamVersion,
		std::unique_ptr<SessionSettings> settings);
	void watchProxyChanges();
	void watchSessionChanges();
#if 0 // mtp
	bool checkForUpdates(const MTP::Response &message);
	bool checkForNewSession(const MTP::Response &message);

	void destroyMtpKeys(MTP::AuthKeysList &&keys);
#endif
	void resetAuthorizationKeys();

	void loggedOut();
	void destroySession(DestroyReason reason);

	[[nodiscard]] std::unique_ptr<Tdb::Account> createTdb();

	const not_null<Domain*> _domain;
	const std::unique_ptr<Storage::Account> _local;
	std::unique_ptr<Tdb::Account> _tdb;
	bool _tdbPaused = false;

	std::unique_ptr<MTP::Instance> _mtp;
#if 0 // mtp
	rpl::variable<MTP::Instance*> _mtpValue;
	std::unique_ptr<MTP::Instance> _mtpForKeysDestroy;
	rpl::event_stream<MTPUpdates> _mtpUpdates;
	rpl::event_stream<> _mtpNewSessionCreated;
#endif
	QString _internalLinksDomain;

	std::unique_ptr<AppConfig> _appConfig;

	std::unique_ptr<Session> _session;
	rpl::variable<Session*> _sessionValue;

	Fn<void(QString)> _handleLoginCode = nullptr;

	UserId _sessionUserId = 0;
	QByteArray _sessionUserSerialized;
	int32 _sessionUserStreamVersion = 0;
	std::unique_ptr<SessionSettings> _storedSessionSettings;
	MTP::Instance::Fields _mtpFields;
	MTP::AuthKeysList _mtpKeysToDestroy;
	bool _loggingOut = false;
	bool _testMode = false;

	ConnectionState _connectionState = ConnectionState::WaitingForNetwork;

	rpl::lifetime _lifetime;

};

} // namespace Main
