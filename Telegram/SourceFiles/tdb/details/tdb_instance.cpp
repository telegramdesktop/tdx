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
#include <crl/crl_semaphore.h>
#include <td/telegram/Client.h>

namespace Tdb::details {
namespace {

using namespace ::td;
using ClientId = ClientManager::ClientId;
namespace api = td_api;

} // namespace

class Instance::Manager final
	: public std::enable_shared_from_this<Manager> {
	struct PrivateTag {
	};

public:
	Manager(PrivateTag);
	~Manager();

	[[nodiscard]] static std::shared_ptr<Manager> Instance();

	[[nodiscard]] ClientId createClient(not_null<Impl*> impl);
	void destroyClient(ClientId id);

	// Thread-safe.
	[[nodiscard]] RequestId allocateRequestId();
	void send(
		ClientId id,
		RequestId allocatedRequestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback);

private:
	void sendToExternal(
		ClientId id,
		api::object_ptr<api::Function> request,
		RequestId requestId = 0);
	void loop();

	void handleUpdateOnMain(ClientId clientId, TLupdate &&update);
	void handleResponseOnMain(
		ClientId clientId,
		RequestId requestId,
		FnMut<void()> &&handler);

	const not_null<ClientManager*> _manager;

	// Lives on the main thread.
	base::flat_map<ClientId, not_null<Impl*>> _clients;

	// Lives on the main thread before _stopRequested, on _thread - after.
	base::flat_set<ClientId> _waiting;

	// Lives on _thread.
	base::flat_set<ClientId> _closed;

	QMutex _mutex;
	base::flat_map<RequestId, ExternalCallback> _callbacks;

	std::atomic<int32> _requestIdCounter = 0;
	std::atomic<ClientId> _closingId = 0;
	crl::semaphore _closing;

	std::thread _thread;
	std::atomic<bool> _stopRequested = false;

};

class Instance::Impl final {
public:
	explicit Impl(InstanceConfig &&config);
	~Impl();

	[[nodiscard]] RequestId allocateRequestId() const;
	void sendPrepared(
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback);
	void cancel(RequestId requestId);
	[[nodiscard]] bool shouldInvokeHandler(RequestId requestId);

	template <
		typename Request,
		typename = std::enable_if_t<!std::is_reference_v<Request>>>
	void send(
			RequestId requestId,
			Request &&request,
			FnMut<void(const typename Request::ResponseType &)> &&done,
			FnMut<void(const Error &)> &&fail) {
		const auto type = request.type();
		return sendPrepared(
			requestId,
			tl_to_generator(std::move(request)),
			PrepareCallback<typename Request::ResponseType>(
				type,
				std::move(done),
				std::move(fail)));
	}

	void handleUpdate(TLupdate &&update);
	[[nodiscard]] rpl::producer<TLupdate> updates() const;

private:
	void sendTdlibParameters();

	const std::shared_ptr<Manager> _manager;
	std::atomic<ClientManager::ClientId> _client;
	InstanceConfig _config;
	QMutex _activeRequestsMutex;
	base::flat_set<RequestId> _activeRequests;
	rpl::event_stream<TLupdate> _updates;

};

std::weak_ptr<Instance::Manager> Instance::ManagerInstance;

std::optional<Error> ParseError(ExternalResponse response) {
	if (response->get_id() != api::error::ID) {
		return std::nullopt;
	}
	return Error(tl_from<TLerror>(response));
}

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

Instance::Manager::Manager(PrivateTag)
: _manager(ClientManager::get_manager_singleton())
, _thread([=] { loop(); }) {
}

Instance::Manager::~Manager() {
	Expects(_stopRequested);
	Expects(!_thread.joinable());
	Expects(_clients.empty());
	Expects(_waiting.empty());
	Expects(_closed.empty());
}

std::shared_ptr<Instance::Manager> Instance::Manager::Instance() {
	auto result = ManagerInstance.lock();
	if (!result) {
		ManagerInstance = result = std::make_shared<Manager>(PrivateTag{});
	}
	return result;
}

[[nodiscard]] ClientId Instance::Manager::createClient(
		not_null<Impl*> impl) {
	Expects(!_stopRequested);

	const auto result = _manager->create_client_id();
	_clients.emplace(result, impl);
	return result;
}

void Instance::Manager::destroyClient(ClientId id) {
	Expects(!_stopRequested);

	LOG(("Tdb Info: Destroying client %1.").arg(id));

	_clients.remove(id);
	_waiting.emplace(id);

	const auto finishing = _clients.empty();
	if (finishing) {
		_stopRequested = true;
	}
	sendToExternal(id, api::make_object<api::close>());
	if (finishing) {
		_thread.join();
	}

	Ensures(!finishing || _waiting.empty());
}

RequestId Instance::Manager::allocateRequestId() {
	return ++_requestIdCounter;
}

void Instance::Manager::send(
		ClientId clientId,
		RequestId allocatedRequestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback) {
	if (callback) {
		QMutexLocker lock(&_mutex);
		_callbacks.emplace(allocatedRequestId, std::move(callback));
	}
	sendToExternal(
		clientId,
		api::object_ptr<api::Function>(request()),
		allocatedRequestId);
}

void Instance::Manager::sendToExternal(
		ClientId id,
		api::object_ptr<api::Function> request,
		RequestId requestId) {
	if (!requestId) {
		requestId = allocateRequestId();
	}
	_manager->send(id, requestId, std::move(request));
}

void Instance::Manager::loop() {
	auto stopping = false;
	while (!stopping || !_waiting.empty()) {
		auto response = _manager->receive(60.);
		if (!response.object) {
			continue;
		}
		if (!stopping && _stopRequested) {
			stopping = true;
			for (const auto clientId : base::take(_closed)) {
				_waiting.remove(clientId);
			}
		}
		if (response.object->get_id() == api::error::ID) {
			const auto error = static_cast<api::error*>(
				response.object.get());
			if (error->code_ == 500
				&& error->message_ == "Request aborted.") {
				continue;
			}
		}
		const auto clientId = response.client_id;
		const auto requestId = RequestId(response.request_id);
		const auto object = response.object.get();
		if (!requestId) {
			auto update = tl_from<TLupdate>(object);
			if (ClientClosedUpdate(update)) {
				LOG(("Tdb Info: Client %1 finished closing.").arg(clientId));
				if (stopping) {
					_waiting.remove(clientId);
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
		_waiting.remove(clientId);
	}
	const auto i = _clients.find(clientId);
	if (i == end(_clients)) {
		return;
	}
	const auto instance = i->second;
	if (closed) {
		_clients.erase(i);
	}
	instance->handleUpdate(std::move(update));
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

Instance::Impl::Impl(InstanceConfig &&config)
: _manager(Manager::Instance())
, _client(_manager->createClient(this))
, _config(std::move(config)) {
	QDir().mkpath(_config.databaseDirectory);
	QDir().mkpath(_config.filesDirectory);
	sendTdlibParameters();
}

Instance::Impl::~Impl() {
	_manager->destroyClient(_client);
}

RequestId Instance::Impl::allocateRequestId() const {
	return _manager->allocateRequestId();
}

void Instance::Impl::sendPrepared(
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback) {
	if (callback) {
		QMutexLocker lock(&_activeRequestsMutex);
		_activeRequests.emplace(requestId);
	}
	crl::async([
		manager = _manager,
		clientId = _client.load(),
		requestId,
		request = std::move(request),
		callback = std::move(callback)
	]() mutable {
		manager->send(
			clientId,
			requestId,
			std::move(request),
			std::move(callback));
	});
}

void Instance::Impl::cancel(RequestId requestId) {
	QMutexLocker lock(&_activeRequestsMutex);
	_activeRequests.remove(requestId);
}

bool Instance::Impl::shouldInvokeHandler(RequestId requestId) {
	QMutexLocker lock(&_activeRequestsMutex);
	return _activeRequests.remove(requestId);
}

void Instance::Impl::handleUpdate(TLupdate &&update) {
	if (ClientClosedUpdate(update)) {
		_client = _manager->createClient(this);
		sendTdlibParameters();
	}
	update.match([&](const TLDupdateAuthorizationState &data) {
		data.vauthorization_state().match([](
			const TLDauthorizationStateWaitTdlibParameters &) {
		}, [&](const auto &) {
			_updates.fire(std::move(update));
		});
	}, [&](const auto &) {
		_updates.fire(std::move(update));
	});
}

rpl::producer<TLupdate> Instance::Impl::updates() const {
	return _updates.events();
}

void Instance::Impl::sendTdlibParameters() {
	const auto fail = [=](Error error) {
		LOG(("Critical Error: setTdlibParameters - %1").arg(error.message));
	};
	send(
		allocateRequestId(),
		TLsetTdlibParameters(
			tl_tdlibParameters(
				tl_bool(_config.testDc),
				tl_string(_config.databaseDirectory),
				tl_string(_config.filesDirectory), // files_directory
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
				tl_bool(false))), // ignore_file_names
		nullptr,
		fail);
}

Instance::Instance(InstanceConfig &&config)
: _impl(std::make_unique<Impl>(std::move(config))) {
}

Instance::~Instance() = default;

RequestId Instance::allocateRequestId() const {
	return _impl->allocateRequestId();
}

void Instance::sendPrepared(
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback) {
	_impl->sendPrepared(
		requestId,
		std::move(request),
		std::move(callback));
}

void Instance::cancel(RequestId requestId) {
	_impl->cancel(requestId);
}

rpl::producer<TLupdate> Instance::updates() const {
	return _impl->updates();
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
