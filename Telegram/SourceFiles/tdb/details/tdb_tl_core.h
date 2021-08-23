/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tl/tl_basic_types.h"
#include "base/match_method.h"

#include <optional>

namespace td::tl {
template <typename T>
class unique_ptr;
} // namespace td::tl

namespace td::td_api {
class Object;
class Function;
} // namespace td::td_api

namespace tl {

enum {
	id_bool = 0x8f99dbc7,
};

} // namespace tl

namespace Tdb {

inline constexpr auto null = std::nullopt;

struct NotSingleDataTypePlaceholder {
};

using TLint32 = tl::int_type;
using TLint53 = tl::int64_type;
using TLint64 = tl::int64_type;
using TLdouble = tl::double_type;
using TLbytes = tl::bytes_type;
template <typename T>
using TLvector = tl::vector_type<T>;

inline constexpr TLint32 tl_int32(int32 value) noexcept {
	return tl::make_int(value);
}
inline constexpr TLint53 tl_int53(int64 value) noexcept {
	return tl::make_int64(value);
}
inline constexpr TLint64 tl_int64(int64 value) noexcept {
	return tl::make_int64(value);
}
inline constexpr TLdouble tl_double(float64 value) noexcept {
	return tl::make_double(value);
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

class TLbool {
public:
	bool v = false;

	constexpr TLbool() noexcept = default;

	constexpr uint32 type() const noexcept {
		return tl::id_bool;
	}

private:
	explicit constexpr TLbool(bool val) noexcept : v(val) {
	}

	friend constexpr TLbool tl_bool(bool v) noexcept;
};
inline constexpr TLbool tl_bool(bool v) noexcept {
	return TLbool(v);
}

class TLstring {
public:
	TLstring() noexcept = default;

	uint32 type() const noexcept {
		return tl::id_string;
	}

	QString v;

private:
	explicit TLstring(QString &&data) noexcept : v(std::move(data)) {
	}

	friend TLstring tl_string(const std::string &v);
	friend TLstring tl_string(const QString &v);
	friend TLstring tl_string(QString &&v);
	friend TLstring tl_string(const char *v);
	friend TLstring tl_string();

};

inline TLstring tl_string(const std::string &v) {
	return TLstring(QString::fromStdString(v));
}
inline TLstring tl_string(const QString &v) {
	return TLstring(QString(v));
}
inline TLstring tl_string(QString &&v) {
	return TLstring(std::move(v));
}
inline TLstring tl_string(const char *v) {
	return TLstring(QString::fromUtf8(v));
}
inline TLstring tl_string() {
	return TLstring(QString());
}

inline bool operator==(const TLstring &a, const TLstring &b) {
	return a.v == b.v;
}
inline bool operator!=(const TLstring &a, const TLstring &b) {
	return a.v != b.v;
}

using ExternalRequest = ::td::td_api::Function*;
using ExternalResponse = const ::td::td_api::Object*;
using ExternalGenerator = Fn<ExternalRequest()>;
using ExternalCallback = FnMut<FnMut<void()>(uint64, ExternalResponse)>;

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

[[nodiscard]] inline QString qs(const TLstring &v) {
	return v.v;
}

} // namespace Tdb
