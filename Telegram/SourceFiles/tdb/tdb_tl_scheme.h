/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb_tl-scheme.h"

namespace Tdb {

struct Error {
	explicit Error(const TLerror &error)
	: code(error.c_error().vcode().v)
	, message(error.c_error().vmessage().v) {
	}
	Error(int code, const QString &message)
	: code(code)
	, message(message) {
	}

	[[nodiscard]] static Error Local(const QString &message) {
		return Error{ -555, message };
	}

	int code = 0;
	QString message;

};

} // namespace Tdb
