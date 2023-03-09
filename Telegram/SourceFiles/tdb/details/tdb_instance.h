/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/details/tdb_tl_core_external.h"
#include "tdb/tdb_request_id.h"
#include "base/expected.h"

namespace Tdb {
class Error;
class TLupdate;
class TLdisableProxy;
class TLaddProxy;
} // namespace Tdb

namespace Tdb::details {

[[nodiscard]] bool ClientClosedUpdate(const TLupdate &update);

void LogError(uint32 type, RequestId requestId, const Error &error);
[[nodiscard]] FnMut<void()> HandleAsError(
	uint32 type,
	RequestId requestId,
	ExternalResponse external,
	FnMut<void(const Error &)> &&fail);

template <typename Response>
[[nodiscard]] ExternalCallback PrepareCallback(
		uint32 type,
		FnMut<void(const Response &)> done,
		FnMut<void(const Error &)> fail) {
	if (!done && !fail) {
		return nullptr;
	}
	return [
		type,
		done = std::move(done),
		fail = std::move(fail)
	](RequestId requestId, ExternalResponse external) mutable
	-> FnMut<void()> {
		auto handled = HandleAsError(
			type,
			requestId,
			external,
			std::move(fail));
		if (handled) {
			return handled;
		} else if (!done) {
			return nullptr;
		}
		return [
			done = std::move(done),
			response = tl_from<Response>(external)
		]() mutable {
			done(response);
		};
	};
}

struct InstanceConfig {
	int32 apiId = 0;
	QString apiHash;
	QString systemLanguageCode;
	QString deviceModel;
	QString systemVersion;
	QString applicationVersion;
	QString databaseDirectory;
	QString filesDirectory;
	QString langpackDirectory;
	QByteArray encryptionKey;
	bool testDc = false;
};

class Instance final {
public:
	// Main thread.
	explicit Instance(InstanceConfig &&config);
	~Instance();

	// Thread safe.
	[[nodiscard]] RequestId allocateRequestId() const;
	template <
		typename Request,
		typename = std::enable_if_t<!std::is_reference_v<Request>>,
		typename = typename Request::ResponseType>
	void send(
			RequestId requestId,
			Request &&request,
			FnMut<void(const typename Request::ResponseType &)> &&done,
			FnMut<void(const Error &)> &&fail) {
		const auto type = request.type();
		sendPrepared(
			requestId,
			tl_to_generator(std::move(request)),
			PrepareCallback<typename Request::ResponseType>(
				type,
				std::move(done),
				std::move(fail)));
	}
	void cancel(RequestId requestId);

	// Main thread.
	[[nodiscard]] rpl::producer<TLupdate> updates() const;
	void logout();
	void reset();

	void setPaused(bool paused);

	void setProxy(std::variant<TLdisableProxy, TLaddProxy> value);

	// Synchronous requests. Use with care!!
	// If request will use the network or even disk it may freeze the app.
	// It expects the _state is Working, instantly fails otherwise.
	// Returns `base::expected<Response, Error>`.
	template <
		typename Request,
		typename = std::enable_if_t<!std::is_reference_v<Request>>,
		typename = typename Request::ResponseType>
	auto sendSync(Request &&request) {
		using Response = typename Request::ResponseType;
		auto container = base::expected<Response, Error>();
		const auto type = request.type();
		sendPreparedSync(
			tl_to_generator(std::move(request)),
			PrepareCallback<Response>(type, [&](const Response &result) {
				container = result;
			}, [&](const Error &error) {
				container = base::make_unexpected(error);
			}));
		return container;
	}

private:
	class Manager;
	class Client;

	void sendPrepared(
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback);
	void sendPreparedSync(
		ExternalGenerator &&request,
		ExternalCallback &&callback);

	std::unique_ptr<Client> _client;

	static std::weak_ptr<Instance::Manager> ManagerInstance;

};

void ExecuteExternal(
	ExternalGenerator &&request,
	ExternalCallback &&callback);

// Returns `base::expected<Response, Error>`.
template <
	typename Request,
	typename = std::enable_if_t<!std::is_reference_v<Request>>,
	typename = typename Request::ResponseType>
auto Execute(Request &&request) {
	using Response = typename Request::ResponseType;
	auto container = base::expected<Response, Error>();
	const auto type = request.type();
	ExecuteExternal(
		tl_to_generator(std::move(request)),
		PrepareCallback<Response>(type, [&](const Response &result) {
			container = result;
		}, [&](const Error &error) {
			container = base::make_unexpected(error);
		}));
	return container;
}

} // namespace Tdb::details
