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

int AppConfig::quoteLengthMax() const {
	return get<int>(u"quote_length_max"_q, 1024);
}

#if 0 // goodToRemove
void AppConfig::refresh(bool force) {
	if (_requestId || !_api) {
		if (force) {
			_pendingRefresh = true;
		}
		return;
	}
	_pendingRefresh = false;
	_requestId = _api->request(MTPhelp_GetAppConfig(
		MTP_int(_hash)
	)).done([=](const MTPhelp_AppConfig &result) {
		_requestId = 0;
		result.match([&](const MTPDhelp_appConfig &data) {
			_hash = data.vhash().v;

			const auto &config = data.vconfig();
			if (config.type() != mtpc_jsonObject) {
				LOG(("API Error: Unexpected config type."));
				return;
			}
			auto was = ignoredRestrictionReasons();

			_data.clear();
			for (const auto &element : config.c_jsonObject().vvalue().v) {
				element.match([&](const MTPDjsonObjectValue &data) {
					_data.emplace_or_assign(qs(data.vkey()), data.vvalue());
				});
			}
			updateIgnoredRestrictionReasons(std::move(was));

			DEBUG_LOG(("getAppConfig result handled."));
			_refreshed.fire({});
		}, [](const MTPDhelp_appConfigNotModified &) {});

		if (base::take(_pendingRefresh)) {
			refresh();
		} else {
			refreshDelayed();
		}
	}).fail([=] {
		_requestId = 0;
		refreshDelayed();
	}).send();
}
#endif

void AppConfig::refresh(bool force) {
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

void AppConfig::updateIgnoredRestrictionReasons(std::vector<QString> was) {
	_ignoreRestrictionReasons = get<std::vector<QString>>(
		u"ignore_restriction_reasons"_q,
		std::vector<QString>());
	ranges::sort(_ignoreRestrictionReasons);
	if (_ignoreRestrictionReasons != was) {
		for (const auto &reason : _ignoreRestrictionReasons) {
			const auto i = ranges::remove(was, reason);
			if (i != end(was)) {
				was.erase(i, end(was));
			} else {
				was.push_back(reason);
			}
		}
		_ignoreRestrictionChanges.fire(std::move(was));
	}
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

base::flat_map<QString, QString> AppConfig::getStringMap(
		const QString &key,
		base::flat_map<QString, QString> &&fallback) const {
	return getValue(key, [&](const MTPJSONValue &value) {
		return value.match([&](const MTPDjsonObject &data) {
			auto result = base::flat_map<QString, QString>();
			result.reserve(data.vvalue().v.size());
			for (const auto &entry : data.vvalue().v) {
				const auto &data = entry.data();
				const auto &value = data.vvalue();
				if (value.type() != mtpc_jsonString) {
					return std::move(fallback);
				}
				result.emplace(
					qs(data.vkey()),
					qs(value.c_jsonString().vvalue()));
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

base::flat_map<QString, QString> AppConfig::getStringMap(
		const QString &key,
		base::flat_map<QString, QString> &&fallback) const {
	return getValue(key, [&](const TLjsonValue &value) {
		return value.match([&](const TLDjsonValueObject &data) {
			auto result = base::flat_map<QString, QString>();
			result.reserve(data.vmembers().v.size());
			for (const auto &entry : data.vmembers().v) {
				const auto &data = entry.data();
				const auto &value = data.vvalue();
				if (value.type() != id_jsonValueString) {
					return std::move(fallback);
				}
				result.emplace(
					qs(data.vkey()),
					qs(value.c_jsonValueString().vvalue()));
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

bool AppConfig::newRequirePremiumFree() const {
	return get<bool>(
		u"new_noncontact_peers_require_premium_without_ownpremium"_q,
		false);
}

} // namespace Main
