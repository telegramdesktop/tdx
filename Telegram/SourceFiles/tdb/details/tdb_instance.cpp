/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/details/tdb_instance.h"

#include <QtCore/QMutex>
#include <crl/crl_semaphore.h>
#include <td/actor/actor.h>
#include <td/telegram/Client.h>

namespace Tdb::details {
namespace {

using namespace ::td;
using ClientId = ClientManager::ClientId;
namespace api = td_api;

} // namespace

std::optional<Error> ParseError(ExternalResponse response) {
	if (response->get_id() != api::error::ID) {
		return std::nullopt;
	}
	return Error(tl_from<TLerror>(response));
}

class Instance::Manager final
	: public std::enable_shared_from_this<Manager> {
	struct PrivateTag {
	};

public:
	Manager(PrivateTag);
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

	const not_null<ClientManager*> _manager;
	std::jthread _thread;
	base::flat_map<ClientId, not_null<Impl*>> _clients;

	QMutex _mutex;
	base::flat_map<RequestId, ExternalCallback> _callbacks;

	std::atomic<uint32> _requestIdCounter = 0;
	std::atomic<ClientId> _closingId = 0;
	crl::semaphore _closing;

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
		return sendPrepared(
			requestId,
			tl_to_generator(std::move(request)),
			PrepareCallback<typename Request::ResponseType>(
				std::move(done),
				std::move(fail)));
	}

	void handleUpdate(TLupdate &&update);
	[[nodiscard]] rpl::producer<TLupdate> updates() const;

private:
	void sendTdlibParameters(InstanceConfig &&config);

	const std::shared_ptr<Manager> _manager;
	ClientManager::ClientId _client;
	QMutex _activeRequestsMutex;
	base::flat_set<RequestId> _activeRequests;
	rpl::event_stream<TLupdate> _updates;

};

std::weak_ptr<Instance::Manager> Instance::ManagerInstance;

Instance::Manager::Manager(PrivateTag)
: _manager(ClientManager::get_manager_singleton())
, _thread([=] { loop(); }) {
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
	const auto result = _manager->create_client_id();
	_clients.emplace(result, impl);
	return result;
}

void Instance::Manager::destroyClient(ClientId id) {
	Expects(!_closingId);

	_closingId = id;
	sendToExternal(id, api::make_object<api::close>());
	_closing.acquire();
	_clients.remove(id);

	if (_clients.empty()) {
		_thread.request_stop();

		// Force wake.
		sendToExternal(id, api::make_object<api::testCallEmpty>());
	}
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
	while (!_thread.get_stop_source().stop_requested()) {
		auto response = _manager->receive(60.);
		if (!response.object) {
			continue;
		}
		if (response.object->get_id() == api::updateAuthorizationState::ID) {
			const auto update = static_cast<api::updateAuthorizationState*>(
				response.object.get());
			const auto state = update->authorization_state_.get();
			if (state->get_id() == api::authorizationStateClosed::ID) {
				Assert(_closingId == response.client_id);
				_closingId = 0;
				_closing.release();
				continue;
			}
		} else if (response.object->get_id() == api::error::ID) {
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
			crl::on_main(weak_from_this(), [
				this,
				clientId,
				update = tl_from<TLupdate>(object)
			]() mutable {
				const auto i = _clients.find(clientId);
				if (i != end(_clients)) {
					i->second->handleUpdate(std::move(update));
				}
			});
			continue;
		}
		QMutexLocker lock(&_mutex);
		auto callback = _callbacks.take(requestId);
		lock.unlock();

		if (!callback) {
			continue;
		}
		crl::on_main(weak_from_this(), [
			this,
			clientId,
			requestId,
			handler = (*callback)({ object })
		]() mutable {
			const auto i = _clients.find(clientId);
			if (i != end(_clients)
				&& i->second->shouldInvokeHandler(requestId)
				&& handler) {
				handler();
			}
		});
	}
}

Instance::Impl::Impl(InstanceConfig &&config)
: _manager(Manager::Instance())
, _client(_manager->createClient(this)) {
	sendTdlibParameters(std::move(config));
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
		clientId = _client,
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

void Instance::Impl::sendTdlibParameters(InstanceConfig &&config) {
	send(
		allocateRequestId(),
		TLsetTdlibParameters(
			tl_tdlibParameters(
				tl_bool(config.testDc),
				tl_string(config.databaseDirectory),
				tl_string(config.filesDirectory), // files_directory
				tl_bool(true), // use_file_database
				tl_bool(true), // use_chat_info_database
				tl_bool(true), // use_message_database
				tl_bool(false), // use_secret_chats
				tl_int32(config.apiId),
				tl_string(config.apiHash),
				tl_string(config.systemLanguageCode),
				tl_string(config.deviceModel),
				tl_string(config.systemVersion),
				tl_string(config.applicationVersion),
				tl_bool(true), // enable_storage_optimizer
				tl_bool(false))), // ignore_file_names
		nullptr,
		nullptr);
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

} // namespace Tdb::details
