/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/tdb_instance.h"

#include "td/actor/actor.h"
#include "td/telegram/Client.h"

#include <crl/crl_semaphore.h>

namespace Tdb {
namespace {

using namespace td;
using ClientId = ClientManager::ClientId;
using RequestId = ClientManager::RequestId;

namespace api = td_api;

} // namespace

class Instance::Manager final
	: public std::enable_shared_from_this<Manager> {
	struct PrivateTag {
	};

public:
	Manager(PrivateTag);
	[[nodiscard]] static std::shared_ptr<Manager> Instance();

	[[nodiscard]] ClientId createClient(not_null<Impl*> impl);
	void destroyClient(ClientId id);

	RequestId send(
		ClientId id,
		api::object_ptr<api::Function> request);

private:
	void loop();

	const not_null<ClientManager*> _manager;
	std::jthread _thread;
	base::flat_map<ClientId, not_null<Impl*>> _clients;
	RequestId _requestId = 0;
	std::atomic<ClientId> _closingId = 0;
	crl::semaphore _closing;

};

class Instance::Impl final {
public:
	explicit Impl(InstanceConfig &&config);
	~Impl();

	void testNetwork(Fn<void(bool)> done);

	void received(RequestId id, api::object_ptr<api::Object> result);

private:
	RequestId send(api::object_ptr<api::Function> request);

	void sendTdlibParameters(InstanceConfig &&config);

	void handleUpdateAuthorizationState(
		api::object_ptr<api::AuthorizationState> state);

	const std::shared_ptr<Manager> _manager;
	ClientManager::ClientId _client;
	base::flat_map<RequestId, Fn<void(bool)>> _callbacks;

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
	_manager->send(id, ++_requestId, api::make_object<api::close>());
	_closing.acquire();
	_clients.erase(id);

	if (_clients.empty()) {
		_thread.request_stop();

		// Force wake.
		_manager->send(
			id,
			++_requestId,
			api::make_object<api::testCallEmpty>());
	}
}

RequestId Instance::Manager::send(
		ClientId id,
		api::object_ptr<api::Function> request) {
	Expects(_clients.contains(id));

	_manager->send(id, ++_requestId, std::move(request));
	return _requestId;
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
		auto handleOnMain = [this, data = std::move(response)]() mutable {
			const auto i = _clients.find(data.client_id);
			if (i == end(_clients)) {
				return;
			}
			i->second->received(data.request_id, std::move(data.object));
		};
		crl::on_main(weak_from_this(), std::move(handleOnMain));
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

void Instance::Impl::testNetwork(Fn<void(bool)> done) {
	const auto id = _manager->send(
		_client,
		api::make_object<api::testNetwork>());
	_callbacks.emplace(id, std::move(done));
}

void Instance::Impl::received(
		RequestId id,
		api::object_ptr<api::Object> result) {
	if (!id) {
		if (result->get_id() == api::updateAuthorizationState::ID) {
			handleUpdateAuthorizationState(
				std::move(static_cast<api::updateAuthorizationState*>(
					result.get())->authorization_state_));
		}
	} else if (const auto callback = _callbacks.take(id)) {
		(*callback)(result->get_id() != api::error::ID);
	}
}

RequestId Instance::Impl::send(api::object_ptr<api::Function> request) {
	return _manager->send(_client, std::move(request));
}

void Instance::Impl::sendTdlibParameters(InstanceConfig &&config) {
	send(api::make_object<api::setTdlibParameters>(
		api::make_object<api::tdlibParameters>(
			false, // use_test_dc
			std::string(), // database_directory
			std::string(), // files_directory
			true, // use_file_database
			true, // use_chat_info_database
			true, // use_message_database
			false, // use_secret_chats
			config.apiId,
			config.apiHash.toStdString(),
			config.systemLanguageCode.toStdString(),
			config.deviceModel.toStdString(),
			config.systemVersion.toStdString(),
			config.applicationVersion.toStdString(),
			true, // enable_storage_optimizer
			false))); // ignore_file_names
}

void Instance::Impl::handleUpdateAuthorizationState(
		api::object_ptr<api::AuthorizationState> state) {
	switch (state->get_id()) {
	case api::authorizationStateWaitTdlibParameters::ID:
		//sendTdlibParameters(); // Should happen only once.
		break;

	case api::authorizationStateWaitEncryptionKey::ID:
		send(api::make_object<api::checkDatabaseEncryptionKey>());
		break;
	}
}

Instance::Instance(InstanceConfig &&config)
: _impl(std::make_unique<Impl>(std::move(config))) {
}

Instance::~Instance() = default;

void Instance::testNetwork(Fn<void(bool)> done) {
	_impl->testNetwork(std::move(done));
}

} // namespace Tdb
