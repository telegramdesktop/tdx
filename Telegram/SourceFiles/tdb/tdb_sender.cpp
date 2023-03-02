/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/tdb_sender.h"

namespace Tdb {

Sender::RequestBuilder::RequestBuilder(
	not_null<Sender*> sender,
	RequestId id) noexcept
: _sender(sender)
, _requestId(id) {
}

auto Sender::RequestBuilder::takeOnFail() noexcept -> FailHandler {
	return std::move(_fail);
}

not_null<Sender*> Sender::RequestBuilder::sender() const noexcept {
	return _sender;
}

RequestId Sender::RequestBuilder::requestId() const noexcept {
	return _requestId;
}

Sender::SentRequestWrap::SentRequestWrap(
	not_null<Sender*> sender,
	RequestId requestId)
: _sender(sender)
, _requestId(requestId) {
}

void Sender::SentRequestWrap::cancel() {
	if (_requestId) {
		_sender->senderRequestCancel(_requestId);
	}
}

Sender::Sender(not_null<details::Instance*> instance) noexcept
: _instance(instance) {
}

Sender::Sender(not_null<Sender*> other) noexcept
: Sender(other->_instance) {
}

Sender::Sender(Sender &other) noexcept
: Sender(other._instance) {
}

void Sender::senderRequestRegister(RequestId requestId) {
	_requests.emplace(_instance, requestId);
}

void Sender::senderRequestHandled(RequestId requestId) {
	const auto i = _requests.find(requestId);
	if (i != end(_requests)) {
		i->handled();
		_requests.erase(i);
	}
}

void Sender::senderRequestCancel(RequestId requestId) {
	const auto i = _requests.find(requestId);
	if (i != end(_requests)) {
		_requests.erase(i);
	}
}

void Sender::requestCancellingDiscard() noexcept {
	for (auto &request : base::take(_requests)) {
		request.handled();
	}
}

} // namespace Tdb
