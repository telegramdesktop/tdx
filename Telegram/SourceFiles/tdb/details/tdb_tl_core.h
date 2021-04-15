/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tl/tl_basic_types.h"
#include "base/match_method.h"

namespace td::tl {
template <typename T>
class unique_ptr;
} // namespace td::tl

namespace td::td_api {
class Object;
class Function;
} // namespace td::td_api

namespace Tdb {

using TLint32 = tl::int_type;
using TLint53 = tl::int64_type;
using TLint64 = tl::int64_type;
using TLdouble = tl::double_type;
using TLstring = tl::string_type;
using TLbytes = tl::bytes_type;
template <typename T>
using TLvector = tl::vector_type<T>;

inline TLint32 tl_int32(int32 value) {
	return tl::make_int(value);
}
inline TLint53 tl_int53(int64 value) {
	return tl::make_int64(value);
}
inline TLint64 tl_int64(int64 value) {
	return tl::make_int64(value);
}
inline TLdouble tl_double(float64 value) {
	return tl::make_double(value);
}
inline TLstring tl_string(const std::string &v) {
	return tl::make_string(v);
}
inline TLstring tl_string(const QString &v) {
	return tl::make_string(v);
}
inline TLbytes tl_string(const QByteArray &v) {
	return tl::make_string(v);
}
inline TLstring tl_string(const char *v) {
	return tl::make_string(v);
}
inline TLstring tl_string() {
	return tl::make_string();
}
inline TLbytes tl_bytes(const QByteArray &v) {
	return tl::make_bytes(v);
}
inline TLbytes tl_bytes(QByteArray &&v) {
	return tl::make_bytes(std::move(v));
}
inline TLbytes tl_bytes() {
	return tl::make_bytes();
}
inline TLbytes tl_bytes(bytes::const_span buffer) {
	return tl::make_bytes(buffer);
}
inline TLbytes tl_bytes(const bytes::vector &buffer) {
	return tl::make_bytes(buffer);
}
template <typename T>
inline TLvector<T> tl_vector(uint32 count) {
	return tl::make_vector<T>(count);
}
template <typename T>
inline TLvector<T> tl_vector(uint32 count, const T &value) {
	return tl::make_vector<T>(count, value);
}
template <typename T>
inline TLvector<T> tl_vector(const QVector<T> &v) {
	return tl::make_vector<T>(v);
}
template <typename T>
inline TLvector<T> tl_vector(QVector<T> &&v) {
	return tl::make_vector<T>(std::move(v));
}
template <typename T>
inline TLvector<T> tl_vector() {
	return tl::make_vector<T>();
}

using ExternalRequest = ::td::td_api::Function*;
using ExternalResponse = const ::td::td_api::Object*;
using ExternalGenerator = Fn<ExternalRequest()>;
using ExternalCallback = FnMut<FnMut<void()>(ExternalResponse)>;

template <typename Request>
[[nodiscard]] ExternalGenerator tl_to_generator(Request &&);

template <typename Response>
[[nodiscard]] Response tl_from(ExternalResponse);

template <typename T>
struct is_TLvector : std::bool_constant<false> {
};

template <typename T>
struct is_TLvector<TLvector<T>> : std::bool_constant<true> {
};

template <typename T>
inline constexpr bool is_TLvector_v = is_TLvector<T>::value;

template <typename T>
struct in_TLvector;

template <typename T>
struct in_TLvector<TLvector<T>> {
	using type = T;
};

template <typename T>
using in_TLvector_t = typename in_TLvector<T>::type;

} // namespace Tdb
