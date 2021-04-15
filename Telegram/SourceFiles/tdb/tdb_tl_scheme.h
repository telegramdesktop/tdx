/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb_tl-scheme.h"

namespace Tdb {

[[nodiscard]] inline TLbool tl_bool(bool value) {
	return value ? tl_boolTrue() : tl_boolFalse();
}

[[nodiscard]] inline bool tl_is_true(const TLbool &value) {
	return (value.type() == id_boolTrue);
}

struct Error {
	explicit Error(const TLerror &error)
	: code(error.c_error().vcode().v)
	, message(error.c_error().vmessage().v) {
	}
	Error(int code, const QString &message)
	: code(code)
	, message(message) {
	}

	int code = 0;
	QString message;

};

} // namespace Tdb
