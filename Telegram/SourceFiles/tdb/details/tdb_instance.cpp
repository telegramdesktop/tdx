/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/details/tdb_instance.h"

#include "base/debug_log.h"

#include <QtCore/QMutex>
#include <QtCore/QDir>

#include <thread>
#include <td/telegram/Client.h>

namespace Tdb::details {
namespace {

constexpr auto kPurgeInvalidLimit = 1024;

using namespace ::td;
using ClientId = ClientManager::ClientId;
namespace api = td_api;

[[nodiscard]] QString BadPostfix() {
	return u"_td_bad"_q;
}

[[nodiscard]] QString ClientKey(const InstanceConfig &config) {
	return config.databaseDirectory;
}

[[nodiscard]] std::optional<Error> ParseError(ExternalResponse response) {
	if (response->get_id() != api::error::ID) {
		return std::nullopt;
	}
	return Error(tl_from<TLerror>(response));
}

template <typename Message>
[[nodiscard]] bool WrongEncryptionKeyError(int code, Message &&message) {
	return (code == 401) && (message == "Wrong database encryption key");
}

template <typename Message>
[[nodiscard]] bool RequestAbortedError(int code, Message &&message) {
	return (code == 500) && (message == "Request aborted.");
}

[[nodiscard]] bool RequestAbortedError(
		const td::ClientManager::Response &response) {
	if (response.object->get_id() == api::error::ID) {
		const auto error = static_cast<const api::error*>(
			response.object.get());
		return RequestAbortedError(error->code_, error->message_);
	}
	return false;
}

bool PurgePath(const QString &path) {
	return QDir(path).removeRecursively();
}

enum class PurgeDatabaseType {
	Test,
	Production,
	Both,
};

bool PurgeDatabase(const QString &directory, PurgeDatabaseType type) {
	for (const auto &name : QDir(directory).entryList(QDir::Files)) {
		const auto clearWith = [&](const QString &postfix) {
			if (!name.startsWith("db" + postfix + ".sqlite")
				&& !name.startsWith("td" + postfix + ".binlog")) {
				return true;
			}
			const auto full = directory + '/' + name;
			auto file = QFile(full);
			if (file.remove() || !file.exists()) {
				return true;
			}
			LOG(("Critical Error: Could not delete file \"%1\".").arg(full));
			return false;
		};
		if ((type != PurgeDatabaseType::Production && !clearWith(u"_test"_q))
			|| (type != PurgeDatabaseType::Test && !clearWith(QString()))) {
			return false;
		}
	}
	return true;
}

void PurgeBad(const QDir &directory) {
	const auto filter = QDir::Dirs | QDir::NoDotAndDotDot;
	for (const auto &name : directory.entryList(filter)) {
		if (name.endsWith(BadPostfix())) {
			PurgePath(directory.absoluteFilePath(name));
		}
	}
}

[[nodiscard]] QString ComputeTempPath(const QString &path, int index) {
	return path + '_' + QString::number(index) + BadPostfix();
}

[[nodiscard]] bool PurgePathAsync(const QString &path) {
	for (auto i = 0; i != kPurgeInvalidLimit; ++i) {
		if (!QDir(path).exists()) {
			return true;
		}
		const auto purging = ComputeTempPath(path, i);
		if (QDir().rename(path, purging)) {
			crl::async([=] {
				PurgePath(purging);
			});
			return true;
		}
	}
	LOG(("Critical Error: Could not rename-for-purge path: %1.").arg(path));
	return false;
}

[[nodiscard]] bool PurgeDatabaseFilesAsync(const QString &directory) {
	const auto filter = QDir::Dirs | QDir::NoDotAndDotDot;
	for (const auto &name : QDir(directory).entryList(filter)) {
		if (!name.endsWith(BadPostfix())
			&& !PurgePathAsync(directory + '/' + name)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool PurgeInvalid(const InstanceConfig &config) {
	const auto db = config.databaseDirectory;
	return PurgeDatabase(db, PurgeDatabaseType::Both)
		&& PurgeDatabaseFilesAsync(db)
		&& PurgePathAsync(config.filesDirectory);
}

} // namespace

class Instance::Manager final
	: public std::enable_shared_from_this<Manager> {
	struct PrivateTag {
	};

public:
	Manager(PrivateTag);
	~Manager();

	[[nodiscard]] static std::shared_ptr<Manager> Instance();

	[[nodiscard]] std::unique_ptr<Client> borrow(InstanceConfig config);
	void free(std::unique_ptr<Client> client);

	[[nodiscard]] ClientId recreateId(not_null<Client*> client);
	void purgeInvalidFinishOnMain(ClientId clientId, const QString &key);

	// Thread-safe.
	[[nodiscard]] RequestId allocateRequestId();
	void enqueueSend(
		ClientId id,
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback);

private:
	void sendToTdManager(
		ClientId id,
		api::object_ptr<api::Function> request,
		RequestId requestId);
	void loop();

	void handleUpdateOnMain(ClientId clientId, TLupdate &&update);
	void handleResponseOnMain(
		ClientId clientId,
		RequestId requestId,
		FnMut<void()> &&handler);

	void clearBadOnce(const InstanceConfig &config);

	const not_null<ClientManager*> _tdmanager;

	// Lives on the main thread.
	base::flat_map<ClientId, not_null<Client*>> _clients;
	base::flat_map<QString, std::unique_ptr<Client>> _waitingToFinish;

	// Lives on the main thread before _stopRequested, on _thread - after.
	base::flat_map<ClientId, RequestId> _waitingForClose;

	// Lives on _thread.
	base::flat_set<ClientId> _closed;

	QMutex _mutex;
	base::flat_map<RequestId, ExternalCallback> _callbacks;

	base::flat_set<QString> _clearingPaths;

	std::atomic<int32> _requestIdCounter = 0;

	std::thread _thread;
	std::unique_ptr<crl::queue> _queue;
	std::atomic<bool> _stopRequested = false;

};

class Instance::Client final {
public:
	Client(ClientId id, InstanceConfig &&config);
	~Client();

	enum class State {
		Working,
		Starting,
		Closing,
		Closed,
		Failed,
		Purging,
		PurgeDone,
		PurgeFail,
	};
	[[nodiscard]] static QString ToString(State state);

	[[nodiscard]] State state() const;
	[[nodiscard]] ClientId id() const;
	[[nodiscard]] QString key() const;

	void replaceConfig(InstanceConfig &&config);

	[[nodiscard]] RequestId allocateRequestId() const;
	void sendPrepared(
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback,
		bool skipStateCheck = false);
	void cancel(RequestId requestId);
	[[nodiscard]] bool shouldInvokeHandler(RequestId requestId);
	void sendPreparedSync(
		ExternalGenerator &&request,
		ExternalCallback &&callback);

	template <
		typename Request,
		typename = std::enable_if_t<!std::is_reference_v<Request>>>
	void send(
			RequestId requestId,
			Request &&request,
			FnMut<void(const typename Request::ResponseType &)> &&done,
			FnMut<void(const Error &)> &&fail,
			bool skipStateCheck = false) {
		const auto type = request.type();
		return sendPrepared(
			requestId,
			tl_to_generator(std::move(request)),
			PrepareCallback<typename Request::ResponseType>(
				type,
				std::move(done),
				std::move(fail)),
			skipStateCheck);
	}

	void handleUpdate(TLupdate &&update);
	void purgeInvalidDone();
	[[nodiscard]] rpl::producer<TLupdate> updates() const;
	void logout();
	void reset();

private:
	struct QueuedRequest {
		ExternalGenerator request;
		ExternalCallback callback;
	};

	void restart();
	void sendTdlibParameters();
	void setIgnorePlatformRestrictions();
	void sendToManager(
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback);
	void purgeInvalid();
	void clearStale();
	void started();

	const std::shared_ptr<Manager> _manager;
	ClientManager::ClientId _id = 0;
	InstanceConfig _config;
	QMutex _activeRequestsMutex;
	base::flat_set<RequestId> _activeRequests;
	base::flat_map<RequestId, QueuedRequest> _queuedRequests;
	rpl::event_stream<TLupdate> _updates;
	std::atomic<State> _state = State::Working;
	std::atomic<bool> _clearingStale = false;

};

std::weak_ptr<Instance::Manager> Instance::ManagerInstance;

bool ClientClosedUpdate(const TLupdate &update) {
	if (update.type() != id_updateAuthorizationState) {
		return false;
	}
	const auto &data = update.c_updateAuthorizationState();
	const auto &state = data.vauthorization_state();
	return (state.type() == id_authorizationStateClosed);
}

void LogError(uint32 type, RequestId requestId, const Error &error) {
	LOG(("Tdb Error (%1) to 0x%2 (requestId %3): ")
		.arg(error.code)
		.arg(type, 0, 16)
		.arg(requestId)
		+ error.message);
}

FnMut<void()> HandleAsError(
		uint32 type,
		RequestId requestId,
		ExternalResponse external,
		FnMut<void(const Error &)> &&fail) {
	auto error = ParseError(external);
	if (!error) {
		return nullptr;
	}
	LogError(type, requestId, *error);
	if (!fail) {
		// If we parsed error, we must return not nullptr.
		// Otherwise we will crash trying to parse response.
		return [] {};
	}
	return[
		fail = std::move(fail),
		error = *error
	]() mutable {
		fail(error);
	};
}

Instance::Manager::Manager(PrivateTag)
: _tdmanager(ClientManager::get_manager_singleton())
, _thread([=] { loop(); })
, _queue(std::make_unique<crl::queue>()) {
}

Instance::Manager::~Manager() {
	Expects(_stopRequested);
	Expects(!_thread.joinable());
	Expects(_clients.empty());
	Expects(_waitingForClose.empty());
	Expects(_waitingToFinish.empty());
	Expects(_closed.empty());

	const auto raw = _queue.get();
	raw->async([moved = std::move(_queue)] {});
}

std::shared_ptr<Instance::Manager> Instance::Manager::Instance() {
	auto result = ManagerInstance.lock();
	if (!result) {
		ManagerInstance = result = std::make_shared<Manager>(PrivateTag{});
	}
	return result;
}

auto Instance::Manager::borrow(InstanceConfig config)
-> std::unique_ptr<Client> {
	Expects(!_stopRequested);

	clearBadOnce(config);

	const auto key = ClientKey(config);
	const auto i = _waitingToFinish.find(key);
	if (i != end(_waitingToFinish)) {
		auto result = std::move(i->second);
		_waitingToFinish.erase(i);
		const auto raw = result.get();
		const auto id = raw->id();
		raw->replaceConfig(std::move(config));
		_clients.emplace(id, raw);

		LOG(("Tdb Info: Reviving client %1 \"%2\".").arg(id).arg(key));

		return result;
	}
	auto result = std::make_unique<Client>(
		_tdmanager->create_client_id(),
		std::move(config));
	const auto raw = result.get();
	const auto id = raw->id();
	_clients.emplace(raw->id(), raw);

	LOG(("Tdb Info: Creating client %1 \"%2\".").arg(id).arg(key));

	return result;
}

void Instance::Manager::free(std::unique_ptr<Client> client) {
	Expects(!_stopRequested);

	const auto raw = client.get();
	const auto state = raw->state();
	const auto key = raw->key();
	const auto id = raw->id();

	LOG(("Tdb Info: Freeing client %1 \"%2\" (state: %3)."
		).arg(id
		).arg(key
		).arg(Client::ToString(state)));

	_clients.remove(id);
	_waitingToFinish.emplace(key, std::move(client));

	const auto finishing = _clients.empty();
	const auto purging = (state == Client::State::Purging)
		|| (state == Client::State::PurgeDone)
		|| (state == Client::State::PurgeFail);
	const auto failed = (state == Client::State::Failed);
	const auto closingId = (purging || failed) ? 0 : allocateRequestId();
	if (closingId) {
		// In case `setTdlibParameters` failed we don't `close`.
		_waitingForClose.emplace(id, closingId);
	}
	if (finishing) {
		LOG(("Tdb Info: All clients freed, finishing."));
		_stopRequested = true;
	}
	if (closingId) {
		LOG(("Tdb Info: Sending close to %1, requestId: %2."
			).arg(id
			).arg(closingId));

		sendToTdManager(id, api::make_object<api::close>(), closingId);
	} else if (finishing) {
		const auto id = _tdmanager->create_client_id();

		LOG(("Tdb Info: Explicitly waking the thread by a new client: %1."
			).arg(id));

		sendToTdManager(
			id,
			api::make_object<api::close>(),
			allocateRequestId());
	}
	if (finishing) {
		LOG(("Tdb Info: Joining the thread."));
		_thread.join();

		LOG(("Tdb Info: Clearing all pending clients."));
		_waitingToFinish.clear();
	}
}

ClientId Instance::Manager::recreateId(not_null<Client*> client) {
	const auto was = client->id();
	const auto now = _tdmanager->create_client_id();

	LOG(("Tdb Info: Replacing client id %1 -> %2.").arg(was).arg(now));

	const auto removed = _clients.remove(was);
	Assert(removed);

	_clients.emplace(now, client);
	return now;
}

RequestId Instance::Manager::allocateRequestId() {
	return ++_requestIdCounter;
}

void Instance::Manager::enqueueSend(
		ClientId clientId,
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback) {
	_queue->async([
		that = shared_from_this(),
		clientId,
		requestId,
		request = std::move(request),
		callback = std::move(callback)
	]() mutable {
		const auto raw = that.get();
		if (callback) {
			QMutexLocker lock(&raw->_mutex);
			raw->_callbacks.emplace(requestId, std::move(callback));
		}
		raw->sendToTdManager(
			clientId,
			api::object_ptr<api::Function>(request()),
			requestId);
	});
}

void Instance::Manager::sendToTdManager(
		ClientId id,
		api::object_ptr<api::Function> request,
		RequestId requestId) {
	_tdmanager->send(id, requestId, std::move(request));
}

void Instance::Manager::loop() {
	auto stopping = false;
	while (!stopping || !_waitingForClose.empty()) {
		auto response = _tdmanager->receive(60.);
		if (!response.object) {
			continue;
		}
		if (!stopping && _stopRequested) {
			stopping = true;
			for (const auto clientId : base::take(_closed)) {
				_waitingForClose.remove(clientId);
			}
		}
		if (RequestAbortedError(response)) {
			continue;
		}
		const auto clientId = response.client_id;
		const auto requestId = RequestId(response.request_id);
		const auto object = response.object.get();
		if (!requestId) {
			auto update = tl_from<TLupdate>(object);
			if (ClientClosedUpdate(update)) {
				LOG(("Tdb Info: Client %1 finished closing.").arg(clientId));
				if (stopping) {
					_waitingForClose.remove(clientId);
				} else {
					_closed.emplace(clientId);
				}
			}
			crl::on_main(weak_from_this(), [
				this,
				clientId,
				update = std::move(update)
			]() mutable {
				handleUpdateOnMain(clientId, std::move(update));
			});
			continue;
		}
		QMutexLocker lock(&_mutex);
		auto callback = _callbacks.take(requestId);
		lock.unlock();

		if (!callback) {
			//if (_waitingForClose[clientId] == requestId) {
			//	_waitingForClose.remove(clientId);
			//}
			if (const auto error = ParseError(object)) {
				LogError(uint32(-1), requestId, *error);
			}
			continue;
		}
		crl::on_main(weak_from_this(), [
			this,
			clientId,
			requestId,
			handler = (*callback)(requestId, object)
		]() mutable {
			handleResponseOnMain(clientId, requestId, std::move(handler));
		});
	}
}

void Instance::Manager::handleUpdateOnMain(
		ClientId clientId,
		TLupdate &&update) {
	const auto closed = ClientClosedUpdate(update);
	if (closed) {
		_waitingForClose.remove(clientId);
	}
	const auto i = _clients.find(clientId);
	if (i != end(_clients)) {
		i->second->handleUpdate(std::move(update));
	} else if (!closed) {
		return;
	}

	for (auto i = begin(_waitingToFinish)
		; i != end(_waitingToFinish)
		; ++i) {
		if (i->second->id() == clientId) {
			LOG(("Tdb Info: Deleting closed client %1 \"%2\"."
				).arg(clientId
				).arg(i->first));

			_waitingToFinish.erase(i);
			break;
		}
	}
}

void Instance::Manager::handleResponseOnMain(
		ClientId clientId,
		RequestId requestId,
		FnMut<void()> &&handler) {
	const auto i = _clients.find(clientId);
	if (i == end(_clients)
		|| !i->second->shouldInvokeHandler(requestId)
		|| !handler) {
		return;
	}
	handler();
}

void Instance::Manager::clearBadOnce(const InstanceConfig &config) {
	if (_clearingPaths.emplace(config.databaseDirectory).second) {
		crl::async([path = config.databaseDirectory] {
			PurgeBad(QDir(path));
		});
	}
	if (_clearingPaths.emplace(config.filesDirectory).second) {
		crl::async([path = config.filesDirectory] {
			auto dir = QDir(path);
			dir.cdUp();
			PurgeBad(dir);
		});
	}
}

void Instance::Manager::purgeInvalidFinishOnMain(
		ClientId clientId,
		const QString &key) {
	const auto i = _clients.find(clientId);
	if (i == end(_clients)) {
		LOG(("Tdb Info: Deleting purged client %1 \"%2\"."
			).arg(clientId
			).arg(key));

		_waitingToFinish.remove(key);
		return;
	}
	i->second->purgeInvalidDone();
}

Instance::Client::Client(ClientId id, InstanceConfig &&config)
: _manager(Manager::Instance())
, _id(id)
, _config(std::move(config)) {
	sendTdlibParameters();
}

Instance::Client::~Client() {
	_state.wait(State::Purging);
	_clearingStale.wait(true);
}

QString Instance::Client::ToString(State state) {
	switch (state) {
	case State::Starting: return u"Starting"_q;
	case State::Working: return u"Working"_q;
	case State::Closing: return u"Closing"_q;
	case State::Closed: return u"Closed"_q;
	case State::Failed: return u"Failed"_q;
	case State::Purging: return u"Purging"_q;
	case State::PurgeDone: return u"PurgeDone"_q;
	case State::PurgeFail: return u"PurgeFail"_q;
	}
	Unexpected("State value in Tdb::Instance::Client::ToString.");
}

Instance::Client::State Instance::Client::state() const {
	return _state.load();
}

ClientId Instance::Client::id() const {
	return _id;
}

QString Instance::Client::key() const {
	return ClientKey(_config);
}

void Instance::Client::replaceConfig(InstanceConfig &&config) {
	_config = std::move(config);
}

RequestId Instance::Client::allocateRequestId() const {
	return _manager->allocateRequestId();
}

void Instance::Client::sendPrepared(
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback,
		bool skipStateCheck) {
	if (callback) {
		QMutexLocker lock(&_activeRequestsMutex);
		_activeRequests.emplace(requestId);
	}
	if (_state == State::Closed) {
		restart();
	}
	if (!skipStateCheck && _state != State::Working) {
		DEBUG_LOG(("Request %1 queued until start working.").arg(requestId));
		_queuedRequests.emplace(requestId, QueuedRequest{
			std::move(request),
			std::move(callback)
		});
		return;
	}
	sendToManager(requestId, std::move(request), std::move(callback));
}

void Instance::Client::sendPreparedSync(
		ExternalGenerator &&request,
		ExternalCallback &&callback) {
	Expects(callback != nullptr);

	if (_state != State::Working) {
		auto error = api::error(500, "Request aborted.");
		callback(0, &error)();
		return;
	}
	auto flag = std::atomic<bool>(false);
	sendToManager(allocateRequestId(), std::move(request), [&, flag = &flag](
			uint64 id,
			ExternalResponse response) mutable {
		// Don't send callback() result to the main thread, invoke instantly.
		callback(id, std::move(response))();
		*flag = true;
		std::atomic_notify_all(flag);
		return FnMut<void()>();
	});
	flag.wait(false);
}

void Instance::Client::sendToManager(
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback) {
	_manager->enqueueSend(
		_id,
		requestId,
		std::move(request),
		std::move(callback));
}

void Instance::Client::cancel(RequestId requestId) {
	{
		QMutexLocker lock(&_activeRequestsMutex);
		_activeRequests.remove(requestId);
	}
	_queuedRequests.remove(requestId);
}

bool Instance::Client::shouldInvokeHandler(RequestId requestId) {
	QMutexLocker lock(&_activeRequestsMutex);
	return _activeRequests.remove(requestId);
}

void Instance::Client::started() {
	if (_state == State::PurgeFail || _state == State::Failed) {
		return;
	}
	_state = State::Working;
	setIgnorePlatformRestrictions();
	for (auto &[requestId, queued] : base::take(_queuedRequests)) {
		sendToManager(
			requestId,
			std::move(queued.request),
			std::move(queued.callback));
	}
}

void Instance::Client::handleUpdate(TLupdate &&update) {
	Expects(_state != State::Purging);

	update.match([&](const TLDupdateAuthorizationState &data) {
		data.vauthorization_state().match([](
			const TLDauthorizationStateWaitTdlibParameters &) {
		}, [&](const TLDauthorizationStateLoggingOut &) {
			_state = State::Closing;
		}, [&](const TLDauthorizationStateClosing &) {
			_state = State::Closing;
		}, [&](const TLDauthorizationStateClosed &) {
			_state = State::Closed;
			_updates.fire(std::move(update));
			if (!_queuedRequests.empty() && _state == State::Closed) {
				restart();
			}
		}, [&](const auto &) {
			started();
			_updates.fire(std::move(update));
		});
	}, [&](const auto &) {
		_updates.fire(std::move(update));
	});
}

void Instance::Client::purgeInvalidDone() {
	if (_state != State::PurgeDone) {
		return;
	}

	LOG(("Tdb Info: Sending `closed` after purge for client %1 \"%2\"."
		).arg(_id
		).arg(ClientKey(_config)));

	handleUpdate(tl_updateAuthorizationState(tl_authorizationStateClosed()));
}

rpl::producer<TLupdate> Instance::Client::updates() const {
	return _updates.events();
}

void Instance::Client::logout() {
	if (_state == State::Purging
		|| _state == State::PurgeDone
		|| _state == State::PurgeFail
		|| _state == State::Failed) {
		return;
	} else if (_state == State::Closed) {
		restart();
		return;
	}
	_state = State::Closing;
	send(allocateRequestId(), TLlogOut(), nullptr, nullptr, true);
}

void Instance::Client::reset() {
	if (_state == State::Purging
		|| _state == State::PurgeDone
		|| _state == State::PurgeFail
		|| _state == State::Failed) {
		return;
	} else if (_state == State::Closed) {
		restart();
		return;
	}
	_state = State::Closing;
	send(allocateRequestId(), TLdestroy(), nullptr, nullptr, true);
}

void Instance::Client::restart() {
	Expects(_state == State::Closed);

	_id = _manager->recreateId(this);
	_state = State::Working;
	sendTdlibParameters();
}

void Instance::Client::sendTdlibParameters() {
	Expects(_state != State::Purging);
	Expects(_state != State::Closed);

	QDir().mkpath(_config.databaseDirectory);
	QDir().mkpath(_config.filesDirectory);

	const auto fail = [=](Error error) {
		LOG(("Critical Error: setTdlibParameters - %1.").arg(error.message));

		if (WrongEncryptionKeyError(error.code, error.message)) {
			purgeInvalid();
		} else {
			_state = State::Failed;
		}
	};
	_clearingStale.wait(true);
	_state = State::Starting;
	send(
		allocateRequestId(),
		TLsetTdlibParameters(
			tl_bool(_config.testDc),
			tl_string(_config.databaseDirectory),
			tl_string(_config.filesDirectory),
			tl_bytes(_config.encryptionKey),
			tl_bool(true), // use_file_database
			tl_bool(true), // use_chat_info_database
			tl_bool(true), // use_message_database
			tl_bool(false), // use_secret_chats
			tl_int32(_config.apiId),
			tl_string(_config.apiHash),
			tl_string(_config.systemLanguageCode),
			tl_string(_config.deviceModel),
			tl_string(_config.systemVersion),
			tl_string(_config.applicationVersion),
			tl_bool(true), // enable_storage_optimizer
			tl_bool(false)), // ignore_file_names
		nullptr,
		fail,
		true);
	clearStale();
}

void Instance::Client::purgeInvalid() {
	_state = State::Purging;
	const auto id = _id;
	const auto key = ClientKey(_config);
	const auto weak = std::weak_ptr(_manager);

	LOG(("Tdb Info: Purging client %1 \"%2\".").arg(id).arg(key));

	_clearingStale.wait(true);
	crl::async([weak, id, key, state = &_state, config = _config] {
		const auto purged = PurgeInvalid(config);

		LOG(("Tdb Info: Purge result %1 for client %2 \"%3\"."
			).arg(purged ? "TRUE" : "FALSE"
			).arg(id
			).arg(key));

		*state = purged ? State::PurgeDone : State::PurgeFail;
		std::atomic_notify_all(state);

		crl::on_main([weak, id, key] {
			if (const auto strong = weak.lock()) {
				strong->purgeInvalidFinishOnMain(id, key);
			} else {
				LOG(("Tdb Info: No manager after purge for client %1 \"%2\"."
					).arg(id
					).arg(key));
			}
		});
	});
}

void Instance::Client::clearStale() {
	_clearingStale = true;
	crl::async([flag = &_clearingStale, config = _config] {
		const auto purgeType = config.testDc
			? PurgeDatabaseType::Production
			: PurgeDatabaseType::Test;
		PurgeDatabase(config.databaseDirectory, purgeType);
		*flag = false;
		std::atomic_notify_all(flag);
	});
}

void Instance::Client::setIgnorePlatformRestrictions() {
#if !defined OS_MAC_STORE && !defined OS_WIN_STORE
	const auto ignore = true;
#else // !OS_MAC_STORE && !OS_WIN_STORE
	const auto ignore = false;
#endif // !OS_MAC_STORE && !OS_WIN_STORE
	send(
		allocateRequestId(),
		TLsetOption(
			tl_string("ignore_platform_restrictions"),
			tl_optionValueBoolean(tl_bool(ignore))),
		nullptr,
		nullptr);
}

Instance::Instance(InstanceConfig &&config)
: _client(Manager::Instance()->borrow(std::move(config))) {
}

Instance::~Instance() {
	Manager::Instance()->free(std::move(_client));
}

RequestId Instance::allocateRequestId() const {
	return _client->allocateRequestId();
}

void Instance::sendPrepared(
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback) {
	_client->sendPrepared(
		requestId,
		std::move(request),
		std::move(callback));
}

void Instance::sendPreparedSync(
		ExternalGenerator &&request,
		ExternalCallback &&callback) {
	_client->sendPreparedSync(std::move(request), std::move(callback));
}

void Instance::cancel(RequestId requestId) {
	_client->cancel(requestId);
}

rpl::producer<TLupdate> Instance::updates() const {
	return _client->updates();
}

void Instance::logout() {
	_client->logout();
}

void Instance::reset() {
	_client->reset();
}

void ExecuteExternal(
		ExternalGenerator &&request,
		ExternalCallback &&callback) {
	const auto result = ClientManager::execute(
		api::object_ptr<api::Function>(request()));
	if (auto handler = callback ? callback(0, result.get()) : nullptr) {
		handler();
	}
}

} // namespace Tdb::details
