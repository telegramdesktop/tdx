/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/tdb_tl_scheme.h"
#include "tdb/tdb_request_id.h"
#include "base/expected.h"

namespace Tdb::details {

[[nodiscard]] std::optional<Error> ParseError(ExternalResponse);
[[nodiscard]] bool ClientClosedUpdate(const TLupdate &update);

void LogError(uint32 type, RequestId requestId, const Error &error);

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
		if (auto error = ParseError(external)) {
			LogError(type, requestId, *error);
			if (!fail) {
				return nullptr;
			}
			return [
				fail = std::move(fail),
				error = *error
			]() mutable {
				fail(error);
			};
		} else {
			if (!done) {
				return nullptr;
			}
			return [
				done = std::move(done),
				response = tl_from<Response>(external)
			]() mutable {
				done(response);
			};
		}
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

private:
	class Manager;
	class Impl;

	void sendPrepared(
		RequestId requestId,
		ExternalGenerator &&request,
		ExternalCallback &&callback);

	const std::unique_ptr<Impl> _impl;

	static std::weak_ptr<Instance::Manager> ManagerInstance;

};

void ExecuteExternal(
	ExternalGenerator &&request,
	ExternalCallback &&callback);

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
