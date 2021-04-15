/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/details/tdb_tl_core_conversion_to.h"

namespace Tdb {

std::string tl_to_simple(const TLstring &value) {
	return value.v.toStdString();
}

std::string tl_to_simple(const TLbytes &value) {
	return value.v.toStdString();
}

std::int32_t tl_to_simple(const TLint32 &value) {
	return value.v;
}

std::int64_t tl_to_simple(const TLint64 &value) {
	return value.v;
}

double tl_to_simple(const TLdouble &value) {
	return value.v;
}

bool tl_to_simple(const TLbool &value) {
	return (value.type() == id_boolTrue);
}

} // namespace Tdb
