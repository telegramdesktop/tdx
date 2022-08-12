/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/details/tdb_instance.h"

namespace Tdb {

class Sender final {
	class RequestBuilder {
	public:
		RequestBuilder(const RequestBuilder &other) = delete;
		RequestBuilder &operator=(const RequestBuilder &other) = delete;
		RequestBuilder &operator=(RequestBuilder &&other) = delete;

	protected:
		template <typename Result>
		using DoneHandler = FnMut<void(const Result&)>;
		using FailHandler = FnMut<void(const Error&)>;

		template <typename ...Args>
		static constexpr bool IsCallable
			= rpl::details::is_callable_plain_v<Args...>;

		explicit RequestBuilder(not_null<Sender*> sender) noexcept;
		RequestBuilder(RequestBuilder &&other) = default;

		template <typename Result, typename Handler>
		[[nodiscard]] DoneHandler<Result> makeDoneHandler(
				Handler &&handler) noexcept {
			return [
				sender = _sender,
				requestId = _requestId,
				handler = std::forward<Handler>(handler)
			](const Result &result) mutable {
				auto onstack = std::move(handler);
				sender->senderRequestHandled(requestId);

				using SingleDataType = typename Result::SingleDataType;
				if (!onstack) {
					return;
				} else if constexpr (IsCallable<
						Handler,
						const Result&,
						RequestId>) {
					onstack(result, requestId);
				} else if constexpr (IsCallable<Handler, const Result&>) {
					onstack(result);
				} else if constexpr (IsCallable<Handler>) {
					onstack();
				} else if constexpr (IsCallable<
						Handler,
						const SingleDataType&,
						RequestId>) {
					onstack(result.data(), requestId);
				} else if constexpr (IsCallable<
						Handler,
						const SingleDataType&>) {
					onstack(result.data());
				} else {
					static_assert(false_t(Handler{}), "Bad done handler.");
				}
			};
		}

		template <typename Handler>
		void setFailHandler(Handler &&handler) noexcept {
			_fail = [
				sender = _sender,
				requestId = _requestId,
				handler = std::forward<Handler>(handler)
			](const Error &error) mutable {
				auto onstack = handler;
				sender->senderRequestHandled(requestId);

				if (!onstack) {
					return;
				} else if constexpr (IsCallable<
						Handler,
						const Error&,
						RequestId>) {
					onstack(error, requestId);
				} else if constexpr (IsCallable<Handler, const Error&>) {
					onstack(error);
				} else if constexpr (IsCallable<Handler>) {
					onstack();
				} else {
					static_assert(false_t(Handler{}), "Bad fail handler.");
				}
			};
		}

		[[nodiscard]] FailHandler takeOnFail() noexcept;
		[[nodiscard]] not_null<Sender*> sender() const noexcept;
		RequestId requestId() const noexcept;

	private:
		not_null<Sender*> _sender;
		RequestId _requestId = 0;
		FailHandler _fail;

	};

public:
	explicit Sender(not_null<details::Instance*> instance) noexcept;
	explicit Sender(not_null<Sender*> other) noexcept;
	explicit Sender(Sender &other) noexcept;

	template <typename Request>
	class SpecificRequestBuilder final : public RequestBuilder {
	private:
		friend class Sender;
		SpecificRequestBuilder(
			not_null<Sender*> sender,
			Request &&request) noexcept
		: RequestBuilder(sender)
		, _request(std::move(request)) {
		}
		SpecificRequestBuilder(SpecificRequestBuilder &&other) = default;

	public:
		using Result = typename Request::ResponseType;
		using SingleDataType = typename Result::SingleDataType;
		[[nodiscard]] SpecificRequestBuilder &done(
				FnMut<void()> callback) {
			_done = makeDoneHandler<Result>(std::move(callback));
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &done(
			FnMut<void(
				const Result &result,
				RequestId requestId)> callback) {
			_done = makeDoneHandler<Result>(std::move(callback));
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &done(
				FnMut<void(const Result &result)> callback) {
			_done = makeDoneHandler<Result>(std::move(callback));
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &done(
			FnMut<void(
				const SingleDataType &result,
				RequestId requestId)> callback) {
			_done = makeDoneHandler<Result>(std::move(callback));
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &done(
				FnMut<void(const SingleDataType &result)> callback) {
			_done = makeDoneHandler<Result>(std::move(callback));
			return *this;
		}

		[[nodiscard]] SpecificRequestBuilder &fail(
				Fn<void()> callback) noexcept {
			setFailHandler(std::move(callback));
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &fail(
			Fn<void(
				const Error &error,
				RequestId requestId)> callback) noexcept {
			setFailHandler(std::move(callback));
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &fail(
				Fn<void(const Error &error)> callback) noexcept {
			setFailHandler(std::move(callback));
			return *this;
		}

		RequestId send() noexcept {
			const auto id = requestId();
			sender()->_instance->send(
				id,
				std::move(_request),
				std::move(_done),
				takeOnFail());
			sender()->senderRequestRegister(id);
			return id;
		}

	private:
		DoneHandler<Result> _done;
		Request _request;

	};

	class SentRequestWrap {
	private:
		friend class Sender;
		SentRequestWrap(not_null<Sender*> sender, RequestId requestId);

	public:
		void cancel();

	private:
		not_null<Sender*> _sender;
		RequestId _requestId = 0;

	};

	template <
		typename Request,
		typename = typename Request::ResponseType>
	[[nodiscard]] SpecificRequestBuilder<Request> request(
		Request &&request) noexcept;
	[[nodiscard]] SentRequestWrap request(RequestId requestId) noexcept;
	[[nodiscard]] auto requestCanceller() noexcept;
	void requestCancellingDiscard() noexcept;

private:
	class RequestWrap {
	public:
		constexpr RequestWrap(
			not_null<details::Instance*> instance,
			RequestId requestId) noexcept;

		RequestWrap(const RequestWrap &other) = delete;
		RequestWrap &operator=(const RequestWrap &other) = delete;
		constexpr RequestWrap(RequestWrap &&other) noexcept;
		RequestWrap &operator=(RequestWrap &&other) noexcept;

		constexpr RequestId id() const noexcept;
		constexpr void handled() const noexcept;

		~RequestWrap();

	private:
		void cancelRequest() noexcept;

		const not_null<details::Instance*> _instance;
		mutable RequestId _id = 0;

	};

	struct RequestWrapComparator {
		using is_transparent = std::true_type;

		struct helper {
			RequestId requestId = 0;

			constexpr helper() noexcept = default;
			constexpr helper(const helper &other) noexcept = default;
			constexpr helper(RequestId requestId) noexcept
			: requestId(requestId) {
			}
			constexpr helper(const RequestWrap &request) noexcept
			: requestId(request.id()) {
			}
			constexpr bool operator<(helper other) const noexcept {
				return requestId < other.requestId;
			}
		};
		constexpr bool operator()(
				const helper &&lhs,
				const helper &&rhs) const {
			return lhs < rhs;
		}

	};

	template <typename Request>
	friend class SpecificRequestBuilder;
	friend class RequestBuilder;
	friend class RequestWrap;
	friend class SentRequestWrap;

	void senderRequestRegister(RequestId requestId);
	void senderRequestHandled(RequestId requestId);
	void senderRequestCancel(RequestId requestId);

	const not_null<details::Instance*> _instance;
	base::flat_set<RequestWrap, RequestWrapComparator> _requests;

};

template <typename Request, typename>
Sender::SpecificRequestBuilder<Request> Sender::request(Request &&request) noexcept {
	static_assert(
		!std::is_reference_v<Request> && !std::is_const_v<Request>,
		"You're supposed to pass non-const rvalue referenced request: "
		"'request(TLsmth())' or 'auto r = TLsmth(); request(std::move(r))'");
	return SpecificRequestBuilder<Request>(this, std::move(request));
}

inline Sender::SentRequestWrap Sender::request(RequestId requestId) noexcept {
	return SentRequestWrap(this, requestId);
}

inline auto Sender::requestCanceller() noexcept {
	return [this](RequestId requestId) {
		request(requestId).cancel();
	};
}

inline constexpr Sender::RequestWrap::RequestWrap(
	not_null<details::Instance*> instance,
	RequestId requestId) noexcept
: _instance(instance)
, _id(requestId) {
}

inline constexpr Sender::RequestWrap::RequestWrap(
	RequestWrap &&other) noexcept
: _instance(other._instance)
, _id(base::take(other._id)) {
}

inline auto Sender::RequestWrap::operator=(
		RequestWrap &&other) noexcept -> RequestWrap & {
	Expects(_instance == other._instance);

	if (_id != other._id) {
		cancelRequest();
		_id = base::take(other._id);
	}
	return *this;
}

inline constexpr RequestId Sender::RequestWrap::id() const noexcept {
	return _id;
}

inline constexpr void Sender::RequestWrap::handled() const noexcept {
	_id = 0;
}

inline Sender::RequestWrap::~RequestWrap() {
	cancelRequest();
}

inline void Sender::RequestWrap::cancelRequest() noexcept {
	if (_id) {
		_instance->cancel(_id);
	}
}

} // namespace Tdb
