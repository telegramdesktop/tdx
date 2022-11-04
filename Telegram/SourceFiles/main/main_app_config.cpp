/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "main/main_app_config.h"

#include "apiwrap.h"
#include "base/call_delayed.h"
#include "main/main_account.h"
#include "ui/chat/chat_style.h"

#include "tdb/tdb_tl_scheme.h"
#include "main/main_session.h"

namespace Main {
namespace {

constexpr auto kRefreshTimeout = 3600 * crl::time(1000);

using namespace Tdb;

} // namespace

AppConfig::AppConfig(not_null<Account*> account) : _account(account) {
	account->sessionChanges(
	) | rpl::filter([=](Session *session) {
		return (session != nullptr);
	}) | rpl::start_with_next([=] {
		if (_api) {
			_api->request(base::take(_requestId)).cancel();
		}
		_api.emplace(_account->session().sender());
		_requestId = 0;
		refresh();
	}, _lifetime);
}

AppConfig::~AppConfig() = default;

void AppConfig::start() {
#if 0 // goodToRemove
	_account->mtpMainSessionValue(
	) | rpl::start_with_next([=](not_null<MTP::Instance*> instance) {
		_api.emplace(instance);
		refresh();
	}, _lifetime);
#endif
}

#if 0 // goodToRemove
void AppConfig::refresh() {
	if (_requestId || !_api) {
		return;
	}
	_requestId = _api->request(MTPhelp_GetAppConfig(
		MTP_int(_hash)
	)).done([=](const MTPhelp_AppConfig &result) {
		_requestId = 0;
		refreshDelayed();
		result.match([&](const MTPDhelp_appConfig &data) {
			_hash = data.vhash().v;

			const auto &config = data.vconfig();
			if (config.type() != mtpc_jsonObject) {
				LOG(("API Error: Unexpected config type."));
				return;
			}
			_data.clear();
			for (const auto &element : config.c_jsonObject().vvalue().v) {
				element.match([&](const MTPDjsonObjectValue &data) {
					_data.emplace_or_assign(qs(data.vkey()), data.vvalue());
				});
			}
			parseColorIndices();
			DEBUG_LOG(("getAppConfig result handled."));
			_refreshed.fire({});
		}, [](const MTPDhelp_appConfigNotModified &) {});
	}).fail([=] {
		_requestId = 0;
		refreshDelayed();
	}).send();
}
#endif

void AppConfig::refresh() {
	if (_requestId || !_api) {
		return;
	}
	_requestId = _api->request(TLgetApplicationConfig(
	)).done([=](const TLjsonValue &result) {
		_requestId = 0;
		refreshDelayed();
		result.match([&](const TLDjsonValueObject &d) {
			_data.clear();
			for (const auto &element : d.vmembers().v) {
				element.match([&](const TLDjsonObjectMember &data) {
					_data.emplace_or_assign(data.vkey().v, data.vvalue());
				});
			}
			DEBUG_LOG(("getAppConfig result handled."));
		}, [](const auto &) {
		});
		_refreshed.fire({});
	}).fail([=](const Error &error) {
		_requestId = 0;
		refreshDelayed();
	}).send();
}

void AppConfig::refreshDelayed() {
	base::call_delayed(kRefreshTimeout, _account, [=] {
		refresh();
	});
}

rpl::producer<> AppConfig::refreshed() const {
	return _refreshed.events();
}

rpl::producer<> AppConfig::value() const {
	return _refreshed.events_starting_with({});
}

#if 0 // goodToRemove
template <typename Extractor>
auto AppConfig::getValue(const QString &key, Extractor &&extractor) const {
	const auto i = _data.find(key);
	return extractor((i != end(_data))
		? i->second
		: MTPJSONValue(MTP_jsonNull()));
}

bool AppConfig::getBool(const QString &key, bool fallback) const {
	return getValue(key, [&](const MTPJSONValue &value) {
		return value.match([&](const MTPDjsonBool &data) {
			return mtpIsTrue(data.vvalue());
		}, [&](const auto &data) {
			return fallback;
		});
	});
}

double AppConfig::getDouble(const QString &key, double fallback) const {
	return getValue(key, [&](const MTPJSONValue &value) {
		return value.match([&](const MTPDjsonNumber &data) {
			return data.vvalue().v;
		}, [&](const auto &data) {
			return fallback;
		});
	});
}

QString AppConfig::getString(
		const QString &key,
		const QString &fallback) const {
	return getValue(key, [&](const MTPJSONValue &value) {
		return value.match([&](const MTPDjsonString &data) {
			return qs(data.vvalue());
		}, [&](const auto &data) {
			return fallback;
		});
	});
}

std::vector<QString> AppConfig::getStringArray(
		const QString &key,
		std::vector<QString> &&fallback) const {
	return getValue(key, [&](const MTPJSONValue &value) {
		return value.match([&](const MTPDjsonArray &data) {
			auto result = std::vector<QString>();
			result.reserve(data.vvalue().v.size());
			for (const auto &entry : data.vvalue().v) {
				if (entry.type() != mtpc_jsonString) {
					return std::move(fallback);
				}
				result.push_back(qs(entry.c_jsonString().vvalue()));
			}
			return result;
		}, [&](const auto &data) {
			return std::move(fallback);
		});
	});
}

std::vector<std::map<QString, QString>> AppConfig::getStringMapArray(
		const QString &key,
		std::vector<std::map<QString, QString>> &&fallback) const {
	return getValue(key, [&](const MTPJSONValue &value) {
		return value.match([&](const MTPDjsonArray &data) {
			auto result = std::vector<std::map<QString, QString>>();
			result.reserve(data.vvalue().v.size());
			for (const auto &entry : data.vvalue().v) {
				if (entry.type() != mtpc_jsonObject) {
					return std::move(fallback);
				}
				auto element = std::map<QString, QString>();
				for (const auto &field : entry.c_jsonObject().vvalue().v) {
					const auto &data = field.c_jsonObjectValue();
					if (data.vvalue().type() != mtpc_jsonString) {
						return std::move(fallback);
					}
					element.emplace(
						qs(data.vkey()),
						qs(data.vvalue().c_jsonString().vvalue()));
				}
				result.push_back(std::move(element));
			}
			return result;
		}, [&](const auto &data) {
			return std::move(fallback);
		});
	});
}
#endif

template <typename Extractor>
auto AppConfig::getValue(const QString &key, Extractor &&extractor) const {
	const auto i = _data.find(key);
	return extractor((i != end(_data))
		? i->second
		: tl_jsonValueNull());
}

bool AppConfig::getBool(const QString &key, bool fallback) const {
	return getValue(key, [&](const TLjsonValue &value) {
		return value.match([&](const TLDjsonValueBoolean &data) {
			return data.vvalue().v;
		}, [&](const auto &data) {
			return fallback;
		});
	});
}

double AppConfig::getDouble(const QString &key, double fallback) const {
	return getValue(key, [&](const TLjsonValue &value) {
		return value.match([&](const TLDjsonValueNumber &data) {
			return data.vvalue().v;
		}, [&](const auto &data) {
			return fallback;
		});
	});
}

QString AppConfig::getString(
		const QString &key,
		const QString &fallback) const {
	return getValue(key, [&](const TLjsonValue &value) {
		return value.match([&](const TLDjsonValueString &data) {
			return data.vvalue().v;
		}, [&](const auto &data) {
			return fallback;
		});
	});
}

std::vector<QString> AppConfig::getStringArray(
		const QString &key,
		std::vector<QString> &&fallback) const {
	return getValue(key, [&](const TLjsonValue &value) {
		return value.match([&](const TLDjsonValueArray &data) {
			auto result = std::vector<QString>();
			result.reserve(data.vvalues().v.size());
			for (const auto &entry : data.vvalues().v) {
				if (entry.type() != id_jsonValueString) {
					return std::move(fallback);
				}
				result.push_back(entry.c_jsonValueString().vvalue().v);
			}
			return result;
		}, [&](const auto &data) {
			return std::move(fallback);
		});
	});
}

std::vector<std::map<QString, QString>> AppConfig::getStringMapArray(
		const QString &key,
		std::vector<std::map<QString, QString>> &&fallback) const {
	auto handleArray = [&](const TLDjsonValueArray &data) {
		auto result = std::vector<std::map<QString, QString>>();
		result.reserve(data.vvalues().v.size());
		for (const auto &entry : data.vvalues().v) {
			if (entry.type() != id_jsonValueObject) {
				return std::move(fallback);
			}
			auto element = std::map<QString, QString>();
			for (const auto &field : entry.c_jsonValueObject().vmembers().v) {
				const auto &data = field.c_jsonObjectMember();
				if (data.vvalue().type() != id_jsonValueString) {
					return std::move(fallback);
				}
				element.emplace(
					data.vkey().v,
					data.vvalue().c_jsonValueString().vvalue().v);
			}
			result.push_back(std::move(element));
		}
		return result;
	};
	return getValue(key, [&](const TLjsonValue &value) {
		return value.match(std::move(handleArray), [&](const auto &data) {
			return std::move(fallback);
		});
	});
}

std::vector<int> AppConfig::getIntArray(
		const QString &key,
		std::vector<int> &&fallback) const {
	return getValue(key, [&](const MTPJSONValue &value) {
		return value.match([&](const MTPDjsonArray &data) {
			auto result = std::vector<int>();
			result.reserve(data.vvalue().v.size());
			for (const auto &entry : data.vvalue().v) {
				if (entry.type() != mtpc_jsonNumber) {
					return std::move(fallback);
				}
				result.push_back(
					int(base::SafeRound(entry.c_jsonNumber().vvalue().v)));
			}
			return result;
		}, [&](const auto &data) {
			return std::move(fallback);
		});
	});
}

bool AppConfig::suggestionCurrent(const QString &key) const {
	return !_dismissedSuggestions.contains(key)
		&& ranges::contains(
			get<std::vector<QString>>(
				u"pending_suggestions"_q,
				std::vector<QString>()),
			key);
}

rpl::producer<> AppConfig::suggestionRequested(const QString &key) const {
	return value(
	) | rpl::filter([=] {
		return suggestionCurrent(key);
	});
}

void AppConfig::dismissSuggestion(const QString &key) {
	Expects(_api.has_value());

	if (!_dismissedSuggestions.emplace(key).second) {
		return;
	}
#if 0 // goodToRemove
	_api->request(MTPhelp_DismissSuggestion(
		MTP_inputPeerEmpty(),
		MTP_string(key)
	)).send();
#endif
}

void AppConfig::parseColorIndices() {
	constexpr auto parseColor = [](const MTPJSONValue &color) {
		if (color.type() != mtpc_jsonString) {
			LOG(("API Error: Bad type for color element."));
			return uint32();
		}
		const auto value = color.c_jsonString().vvalue().v;
		if (value.size() != 6) {
			LOG(("API Error: Bad length for color element: %1"
				).arg(qs(value)));
			return uint32();
		}
		const auto hex = [](char ch) {
			return (ch >= 'a' && ch <= 'f')
				? (ch - 'a' + 10)
				: (ch >= 'A' && ch <= 'F')
				? (ch - 'A' + 10)
				: (ch >= '0' && ch <= '9')
				? (ch - '0')
				: 0;
		};
		auto result = (uint32(1) << 24);
		for (auto i = 0; i != 6; ++i) {
			result |= (uint32(hex(value[i])) << ((5 - i) * 4));
		}
		return result;
	};

	struct ParsedColor {
		uint8 colorIndex = Ui::kColorIndexCount;
		std::array<uint32, Ui::kColorPatternsCount> colors;

		explicit operator bool() const {
			return colorIndex < Ui::kColorIndexCount;
		}
	};
	const auto parseColors = [&](const MTPJSONObjectValue &element) {
		const auto &data = element.data();
		if (data.vvalue().type() != mtpc_jsonArray) {
			LOG(("API Error: Bad value for peer_colors element."));
			return ParsedColor();
		}
		const auto &list = data.vvalue().c_jsonArray().vvalue().v;
		if (list.empty() || list.size() > Ui::kColorPatternsCount) {
			LOG(("API Error: Bad count for peer_colors element: %1"
				).arg(list.size()));
			return ParsedColor();
		}
		const auto index = data.vkey().v.toInt();
		if (index < Ui::kSimpleColorIndexCount
			|| index >= Ui::kColorIndexCount) {
			LOG(("API Error: Bad index for peer_colors element: %1"
				).arg(qs(data.vkey().v)));
			return ParsedColor();
		}
		auto result = ParsedColor{ .colorIndex = uint8(index) };
		auto fill = result.colors.data();
		for (const auto &color : list) {
			*fill++ = parseColor(color);
		}
		return result;
	};
	const auto checkColorsObjectType = [&](const MTPJSONValue &value) {
		if (value.type() != mtpc_jsonObject) {
			if (value.type() != mtpc_jsonArray
				|| !value.c_jsonArray().vvalue().v.empty()) {
				LOG(("API Error: Bad value for [dark_]peer_colors."));
			}
			return false;
		}
		return true;
	};

	auto colors = std::make_shared<
		std::array<Ui::ColorIndexData, Ui::kColorIndexCount>>();
	getValue(u"peer_colors"_q, [&](const MTPJSONValue &value) {
		if (!checkColorsObjectType(value)) {
			return;
		}
		for (const auto &element : value.c_jsonObject().vvalue().v) {
			if (const auto parsed = parseColors(element)) {
				auto &fields = (*colors)[parsed.colorIndex];
				fields.dark = fields.light = parsed.colors;
			}
		}
	});
	getValue(u"dark_peer_colors"_q, [&](const MTPJSONValue &value) {
		if (!checkColorsObjectType(value)) {
			return;
		}
		for (const auto &element : value.c_jsonObject().vvalue().v) {
			if (const auto parsed = parseColors(element)) {
				(*colors)[parsed.colorIndex].dark = parsed.colors;
			}
		}
	});

	if (!_colorIndicesCurrent) {
		_colorIndicesCurrent = std::make_unique<Ui::ColorIndicesCompressed>(
			Ui::ColorIndicesCompressed{ std::move(colors) });
		_colorIndicesChanged.fire({});
	} else if (*_colorIndicesCurrent->colors != *colors) {
		_colorIndicesCurrent->colors = std::move(colors);
		_colorIndicesChanged.fire({});
	}
}

auto AppConfig::colorIndicesValue() const
-> rpl::producer<Ui::ColorIndicesCompressed> {
	return rpl::single(_colorIndicesCurrent
		? *_colorIndicesCurrent
		: Ui::ColorIndicesCompressed()
	) | rpl::then(_colorIndicesChanged.events() | rpl::map([=] {
		return *_colorIndicesCurrent;
	}));
}

} // namespace Main
