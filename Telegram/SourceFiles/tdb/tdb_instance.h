/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/tdb_tl_scheme.h"

namespace Tdb::details {

[[nodiscard]] std::optional<Error> ParseError(ExternalResponse);

template <typename Response>
[[nodiscard]] ExternalCallback PrepareCallback(
		FnMut<void(const Response &)> done,
		FnMut<void(const Error &)> fail) {
	if (!done && !fail) {
		return nullptr;
	}
	return [
		done = std::move(done),
		fail = std::move(fail)
	](ExternalResponse external) mutable -> FnMut<void()> {
		if (auto error = ParseError(external)) {
			if (!fail) {
				return nullptr;
			}
			// #TODO tdlib log error
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

} // namespace Tdb::details

namespace Tdb {

using RequestId = uint64;

struct InstanceConfig {
	int32 apiId = 0;
	QString apiHash;
	QString systemLanguageCode;
	QString deviceModel;
	QString systemVersion;
	QString applicationVersion;
};

class Instance final {
public:
	explicit Instance(InstanceConfig &&config);
	~Instance();

	template <
		typename Request,
		typename = std::enable_if_t<!std::is_reference_v<Request>>>
	RequestId send(
			Request &&request,
			FnMut<void(const typename Request::ResponseType &)> &&done,
			FnMut<void(const Error &)> &&fail) {
		return sendPrepared(
			tl_to_generator(std::move(request)),
			details::PrepareCallback<typename Request::ResponseType>(
				std::move(done),
				std::move(fail)));
	}
	void cancel(RequestId requestId);

private:
	class Manager;
	class Impl;

	RequestId sendPrepared(
		ExternalGenerator &&request,
		ExternalCallback &&callback);

	const std::unique_ptr<Impl> _impl;

	static std::weak_ptr<Instance::Manager> ManagerInstance;

};

} // namespace Tdb
