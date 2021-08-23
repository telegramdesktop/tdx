/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/details/tdb_tl_core_conversion_from.h"

namespace Tdb {

TLstring tl_from_string(const std::string &value) {
	return tl_string(value);
}

TLbytes tl_from_simple(const std::string &value) {
	return tl_bytes(QByteArray::fromStdString(value));
}

TLint32 tl_from_simple(std::int32_t value) {
	return tl_int32(value);
}

TLint64 tl_from_simple(std::int64_t value) {
	return tl_int64(value);
}

TLdouble tl_from_simple(double value) {
	return tl_double(value);
}

TLbool tl_from_simple(bool value) {
	return tl_bool(value);
}

} // namespace Tdb
