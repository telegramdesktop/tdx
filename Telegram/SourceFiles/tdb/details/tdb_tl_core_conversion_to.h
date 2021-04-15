/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/details/tdb_tl_core.h"
#include "tdb_tl-scheme.h"

namespace Tdb {

std::string tl_to_simple(const TLstring &value);
std::int32_t tl_to_simple(const TLint32 &value);
std::int64_t tl_to_simple(const TLint64 &value);
double tl_to_simple(const TLdouble &value);
bool tl_to_simple(const TLbool &value);

template <typename T>
struct tl_to_type
	: std::type_identity<
		::td::tl::unique_ptr<
			std::remove_pointer_t<decltype(tl_to(std::declval<T>()))>>> {
};

template <typename T>
using tl_to_type_t = typename tl_to_type<T>::type;

template <>
struct tl_to_type<TLstring> : std::type_identity<std::string> {
};

template <>
struct tl_to_type<TLint32> : std::type_identity<std::int32_t> {
};

template <>
struct tl_to_type<TLint64> : std::type_identity<std::int64_t> {
};

template <>
struct tl_to_type<TLdouble> : std::type_identity<double> {
};

template <>
struct tl_to_type<TLbool> : std::type_identity<bool> {
};

template <typename T>
struct tl_to_type<TLvector<T>>
	: std::type_identity<std::vector<tl_to_type_t<T>>> {
};

template <typename T>
auto tl_to_vector_optional(const TLvector<std::optional<T>> &value) {
	using U = tl_to_type_t<T>;
	auto result = std::vector<U>();
	result.reserve(value.v.size());
	for (const auto &element : value.v) {
		result.push_back(U(element ? tl_to(*element) : nullptr));
	}
	return result;
}

template <typename T>
auto tl_to_vector(const TLvector<T> &value) {
	constexpr bool simple = std::is_same_v<TLint32, T>
		|| std::is_same_v<TLint64, T>
		|| std::is_same_v<TLstring, T>
		|| std::is_same_v<TLdouble, T>
		|| std::is_same_v<TLbool, T>;
	constexpr bool vector = !simple && is_TLvector_v<T>;
	using U = tl_to_type_t<T>;
	auto result = std::vector<U>();
	result.reserve(value.v.size());
	for (const auto &element : value.v) {
		if constexpr (simple) {
			result.push_back(tl_to_simple(element));
		} else if constexpr (vector) {
			result.push_back(tl_to_vector(element));
		} else {
			result.push_back(U(tl_to(element)));
		}
	}
	return result;
}

} // namespace Tdb
