/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace td::td_api {
class Object;
class Function;
} // namespace td::td_api

namespace Tdb {

using ExternalRequest = ::td::td_api::Function*;
using ExternalResponse = const ::td::td_api::Object*;
using ExternalGenerator = Fn<ExternalRequest()>;
using ExternalCallback = FnMut<FnMut<void()>(uint64, ExternalResponse)>;

template <typename Request>
[[nodiscard]] ExternalGenerator tl_to_generator(Request &&);

template <typename Response>
[[nodiscard]] Response tl_from(ExternalResponse);

} // namespace Tdb
