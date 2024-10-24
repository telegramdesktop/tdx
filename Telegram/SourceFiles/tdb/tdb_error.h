/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Tdb {

class TLerror;

class Error {
public:
	Error(int code, const QString &message)
	: code(code)
	, message(message) {
	}
	Error(const Error &other) = default;
	Error &operator=(const Error &other) {
		const_cast<int&>(code) = other.code;
		const_cast<QString&>(message) = other.message;
		return *this;
	}

	[[nodiscard]] static Error Local(const QString &message) {
		return { -555, message };
	}

	const int code = 0;
	const QString message;

};

[[nodiscard]] inline bool IsFloodError(const QString &type) {
	// See: td/telegram/net/NetQueryDelayer.cpp.
	return type.startsWith(u"Too Many Requests:"_q);
}

[[nodiscard]] inline bool IsFloodError(const Error &error) {
	// See: td/telegram/net/NetQueryDelayer.cpp.
	return (error.code == 429);
}

} // namespace Tdb
