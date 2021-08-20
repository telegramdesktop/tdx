/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb_tl-scheme.h"

namespace Tdb {

template <typename ResultType>
ResultType OptionValue(TLoptionValue option) {
	using OptionType = std::conditional_t<
		std::is_same_v<ResultType, bool>,
		TLDoptionValueBoolean,
		std::conditional_t<std::is_same_v<ResultType, int64>,
		TLDoptionValueInteger,
		std::conditional_t<std::is_same_v<ResultType, QString>,
		TLDoptionValueString,
		TLDoptionValueEmpty>>>;
	return option.match([](const OptionType &data) {
		return data.vvalue().v;
	}, [](const auto &) {
		Unexpected("Tdb::OptionValue wrong type.");
		return ResultType{};
	});
}

template <typename ResultType>
std::optional<ResultType> OptionValueMaybe(TLoptionValue option) {
	using OptionType = std::conditional_t<
		std::is_same_v<ResultType, bool>,
		TLDoptionValueBoolean,
		std::conditional_t<std::is_same_v<ResultType, int64>,
		TLDoptionValueInteger,
		std::conditional_t<std::is_same_v<ResultType, QString>,
		TLDoptionValueString,
		TLDoptionValueEmpty>>>;
	return option.match([](
			const OptionType &data) -> std::optional<ResultType> {
		return data.vvalue().v;
	}, [](const auto &) {
		return std::nullopt;
	});
}

} // namespace Tdb
