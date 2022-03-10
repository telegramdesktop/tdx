/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "lang/lang_cloud_manager.h"

#include "lang/lang_instance.h"
#include "lang/lang_file_parser.h"
#include "lang/lang_text_entity.h"
#include "tdb/tdb_option.h"
#include "mtproto/mtp_instance.h"
#include "storage/localstorage.h"
#include "storage/storage_account.h"
#include "core/application.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "ui/boxes/confirm_box.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/widgets/labels.h"
#include "ui/text/text_utilities.h"
#include "core/file_utilities.h"
#include "core/click_handler_types.h"
#include "boxes/abstract_box.h" // Ui::hideLayer().
#include "styles/style_layers.h"

namespace Lang {
namespace {

class ConfirmSwitchBox : public Ui::BoxContent {
public:
	ConfirmSwitchBox(
		QWidget*,
#if 0 // goodToRemove
		const MTPDlangPackLanguage &data,
#endif
		const Tdb::TLDlanguagePackInfo &data,
		Fn<void()> apply);

protected:
	void prepare() override;

private:
	QString _name;
	int _percent = 0;
	bool _official = false;
	QString _editLink;
	Fn<void()> _apply;

};

class NotReadyBox : public Ui::BoxContent {
public:
	NotReadyBox(
		QWidget*,
#if 0 // goodToRemove
		const MTPDlangPackLanguage &data);
#endif
		const Tdb::TLDlanguagePackInfo &data);

protected:
	void prepare() override;

private:
	QString _name;
	QString _editLink;

};

ConfirmSwitchBox::ConfirmSwitchBox(
	QWidget*,
	const Tdb::TLDlanguagePackInfo &data,
	Fn<void()> apply)
#if 0 // goodToRemove
	const MTPDlangPackLanguage &data,
	Fn<void()> apply)
: _name(qs(data.vnative_name()))
, _percent(data.vtranslated_count().v * 100 / data.vstrings_count().v)
, _official(data.is_official())
, _editLink(qs(data.vtranslations_url()))
, _apply(std::move(apply)) {
#endif
: _name(data.vnative_name().v)
, _percent(data.vtranslated_string_count().v * 100
	/ data.vtotal_string_count().v)
, _official(data.vis_official().v)
, _editLink(data.vtranslation_url().v)
, _apply(std::move(apply)) {
}

void ConfirmSwitchBox::prepare() {
	setTitle(tr::lng_language_switch_title());

	auto text = (_official
		? tr::lng_language_switch_about_official
		: tr::lng_language_switch_about_unofficial)(
			lt_lang_name,
			rpl::single(Ui::Text::Bold(_name)),
			lt_percent,
			rpl::single(Ui::Text::Bold(QString::number(_percent))),
			lt_link,
			tr::lng_language_switch_link() | Ui::Text::ToLink(_editLink),
			Ui::Text::WithEntities);
	const auto content = Ui::CreateChild<Ui::PaddingWrap<Ui::FlatLabel>>(
		this,
		object_ptr<Ui::FlatLabel>(
			this,
			std::move(text),
			st::boxLabel),
		QMargins{ st::boxPadding.left(), 0, st::boxPadding.right(), 0 });
	content->entity()->setLinksTrusted();

	addButton(tr::lng_language_switch_apply(), [=] {
		const auto apply = _apply;
		closeBox();
		apply();
	});
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	content->resizeToWidth(st::boxWideWidth);
	content->heightValue(
	) | rpl::start_with_next([=](int height) {
		setDimensions(st::boxWideWidth, height);
	}, lifetime());
}

NotReadyBox::NotReadyBox(
	QWidget*,
	const Tdb::TLDlanguagePackInfo &data)
#if 0 // goodToRemove
	const MTPDlangPackLanguage &data)
: _name(qs(data.vnative_name()))
, _editLink(qs(data.vtranslations_url())) {
#endif
: _name(data.vnative_name().v)
, _editLink(data.vtranslation_url().v) {
}

void NotReadyBox::prepare() {
	setTitle(tr::lng_language_not_ready_title());

	auto text = tr::lng_language_not_ready_about(
		lt_lang_name,
		rpl::single(_name) | Ui::Text::ToWithEntities(),
		lt_link,
		tr::lng_language_not_ready_link() | Ui::Text::ToLink(_editLink),
		Ui::Text::WithEntities);
	const auto content = Ui::CreateChild<Ui::PaddingWrap<Ui::FlatLabel>>(
		this,
		object_ptr<Ui::FlatLabel>(
			this,
			std::move(text),
			st::boxLabel),
		QMargins{ st::boxPadding.left(), 0, st::boxPadding.right(), 0 });
	content->entity()->setLinksTrusted();

	addButton(tr::lng_box_ok(), [=] { closeBox(); });

	content->resizeToWidth(st::boxWidth);
	content->heightValue(
	) | rpl::start_with_next([=](int height) {
		setDimensions(st::boxWidth, height);
	}, lifetime());
}

} // namespace

#if 0 // goodToRemove
Language ParseLanguage(const MTPLangPackLanguage &data) {
	return data.match([](const MTPDlangPackLanguage &data) {
		return Language{
			qs(data.vlang_code()),
			qs(data.vplural_code()),
			qs(data.vbase_lang_code().value_or_empty()),
			qs(data.vname()),
			qs(data.vnative_name())
		};
	});
}
#endif

Language ParseLanguage(const Tdb::TLlanguagePackInfo &result) {
	return result.match([](const Tdb::TLDlanguagePackInfo &data) {
		return Language{
			data.vid().v,
			data.vplural_code().v,
			data.vbase_language_pack_id().v,
			data.vname().v,
			data.vnative_name().v,
		};
	});
}

CloudManager::CloudManager(Instance &langpack)
: _langpack(langpack) {
#if 0 // goodToRemove
	const auto mtpLifetime = _lifetime.make_state<rpl::lifetime>();
	Core::App().domain().activeValue(
	) | rpl::filter([=](Main::Account *account) {
		return (account != nullptr);
	}) | rpl::start_with_next_done([=](Main::Account *account) {
		*mtpLifetime = account->mtpMainSessionValue(
		) | rpl::start_with_next([=](not_null<MTP::Instance*> instance) {
			_api.emplace(instance);
			resendRequests();
		});
	}, [=] {
		_api.reset();
	}, _lifetime);
#endif

	Core::App().domain().activeValue(
	) | rpl::filter([=](Main::Account *account) {
		return (account != nullptr);
	}) | rpl::start_with_next_done([=](Main::Account *account) {
		_api.emplace(account->sender());
		init();
	}, [=] {
		_api.reset();
	}, _lifetime);
}

void CloudManager::init() {
	using namespace Tdb;
	const auto setDefaultValue = [&](
			const QString &option,
			const QString &defaultValue) {
		_api->request(TLgetOption(
			tl_string(option)
		)).done([=](const TLoptionValue &result) {
			result.match([&](const TLDoptionValueEmpty &data) {
				_api->request(TLsetOption(
					tl_string(option),
					tl_optionValueString(tl_string(defaultValue))
				)).send();
			}, [](const auto &) {
			});
		}).send();
	};
	setDefaultValue(
		"language_pack_database_path",
		// doLater better.
		Core::App().domain().active().local().libDatabasePath() + "/lang");
	setDefaultValue("localization_target", Lang::CloudLangPackName());
	setDefaultValue("language_pack_id", Lang::DefaultLanguageId());
}

Pack CloudManager::packTypeFromId(const QString &id) const {
	if (id == LanguageIdOrDefault(_langpack.id())) {
		return Pack::Current;
	} else if (id == _langpack.baseId()) {
		return Pack::Base;
	}
	return Pack::None;
}

rpl::producer<> CloudManager::languageListChanged() const {
	return _languageListChanged.events();
}

rpl::producer<> CloudManager::firstLanguageSuggestion() const {
	return _firstLanguageSuggestion.events();
}

#if 0 // goodToRemove
void CloudManager::requestLangPackDifference(const QString &langId) {
	Expects(!langId.isEmpty());

	if (langId == LanguageIdOrDefault(_langpack.id())) {
		requestLangPackDifference(Pack::Current);
	} else {
		requestLangPackDifference(Pack::Base);
	}
}
#endif

void CloudManager::requestLangPackStrings(const QString &langId) {
	Expects(!langId.isEmpty());

	if (langId == LanguageIdOrDefault(_langpack.id())) {
		requestLangPackStrings(Pack::Current);
	} else {
		requestLangPackStrings(Pack::Base);
	}
}

mtpRequestId &CloudManager::packRequestId(Pack pack) {
	return (pack != Pack::Base)
		? _langPackRequestId
		: _langPackBaseRequestId;
}

mtpRequestId CloudManager::packRequestId(Pack pack) const {
	return (pack != Pack::Base)
		? _langPackRequestId
		: _langPackBaseRequestId;
}

#if 0 // goodToRemove
void CloudManager::requestLangPackDifference(Pack pack) {
	if (!_api) {
		return;
	}
	_api->request(base::take(packRequestId(pack))).cancel();
	if (_langpack.isCustom()) {
		return;
	}

	const auto version = _langpack.version(pack);
	const auto code = _langpack.cloudLangCode(pack);
	if (code.isEmpty()) {
		return;
	}
	if (version > 0) {
		packRequestId(pack) = _api->request(MTPlangpack_GetDifference(
			MTP_string(CloudLangPackName()),
			MTP_string(code),
			MTP_int(version)
		)).done([=](const MTPLangPackDifference &result) {
			packRequestId(pack) = 0;
			applyLangPackDifference(result);
		}).fail([=] {
			packRequestId(pack) = 0;
		}).send();
	} else {
		packRequestId(pack) = _api->request(MTPlangpack_GetLangPack(
			MTP_string(CloudLangPackName()),
			MTP_string(code)
		)).done([=](const MTPLangPackDifference &result) {
			packRequestId(pack) = 0;
			applyLangPackDifference(result);
		}).fail([=] {
			packRequestId(pack) = 0;
		}).send();
	}
}
#endif

void CloudManager::requestLangPackStrings(Pack pack) {
	if (!_api) {
		return;
	}
	if (_langpack.isCustom()) {
		return;
	}

	const auto code = _langpack.cloudLangCode(pack);
	if (code.isEmpty()) {
		return;
	}
	using namespace Tdb;
	packRequestId(pack) = _api->request(TLgetLanguagePackStrings(
		tl_string(code),
		tl_vector<TLstring>()
	)).done([=](const TLlanguagePackStrings &result) {
		packRequestId(pack) = 0;
		auto strings = result.match([](const TLDlanguagePackStrings &data) {
			return data.vstrings();
		});
		applyStrings(pack, std::move(strings));
	}).fail([=](const Error &) {
		packRequestId(pack) = 0;
	}).send();
}

void CloudManager::setSuggestedLanguage(const QString &langCode) {
	if (Lang::LanguageIdOrDefault(langCode) != Lang::DefaultLanguageId()) {
		_suggestedLanguage = langCode;
	} else {
		_suggestedLanguage = QString();
	}

	if (!_languageWasSuggested) {
		_languageWasSuggested = true;
		_firstLanguageSuggestion.fire({});

		if (Core::App().offerLegacyLangPackSwitch()
			&& _langpack.id().isEmpty()
			&& !_suggestedLanguage.isEmpty()) {
			_offerSwitchToId = _suggestedLanguage;
			offerSwitchLangPack();
		}
	}
}

#if 0 // goodToRemove
void CloudManager::setCurrentVersions(int version, int baseVersion) {
	const auto check = [&](Pack pack, int version) {
		if (version > _langpack.version(pack) && !packRequestId(pack)) {
			requestLangPackDifference(pack);
		}
	};
	check(Pack::Current, version);
	check(Pack::Base, baseVersion);
}

void CloudManager::applyLangPackDifference(
		const MTPLangPackDifference &difference) {
	Expects(difference.type() == mtpc_langPackDifference);

	if (_langpack.isCustom()) {
		return;
	}

	const auto &langpack = difference.c_langPackDifference();
	const auto langpackId = qs(langpack.vlang_code());
	const auto pack = packTypeFromId(langpackId);
	if (pack != Pack::None) {
		applyLangPackData(pack, langpack);
		if (_restartAfterSwitch) {
			restartAfterSwitch();
		}
	} else {
		LOG(("Lang Warning: "
			"Ignoring update for '%1' because our language is '%2'").arg(
			langpackId,
			_langpack.id()));
	}
}
#endif

void CloudManager::apply(const Tdb::TLDupdateLanguagePackStrings &result) {
	if (_langpack.isCustom()) {
		return;
	}

	const auto langpackId = result.vlanguage_pack_id().v;
	const auto pack = packTypeFromId(langpackId);
	if (pack != Pack::None) {
		applyStrings(pack, result.vstrings());
	} else {
		LOG(("Lang Warning: "
			"Ignoring update for '%1' because our language is '%2'").arg(
			langpackId,
			_langpack.id()));
	}
}

void CloudManager::applyStrings(
		Pack pack,
		const Tdb::TLvector<Tdb::TLlanguagePackString> &strings) {
	_langpack.apply(pack, strings);
	Local::writeLangPack();
	if (_restartAfterSwitch) {
		restartAfterSwitch();
	}
}

bool CloudManager::apply(const Tdb::TLDupdateOption &result) {
	if (result.vname().v == u"language_pack_id"_q) {
#if 0 // doLater - solution without localstorage for the future.
		const auto langPackId = Tdb::OptionValue<QString>(result.vvalue());

		_api->request(Tdb::TLgetLocalizationTargetInfo(
			Tdb::tl_bool(true) // Only local.
		)).done([=](const Tdb::TLlocalizationTargetInfo &info) {
			using Info = Tdb::TLDlocalizationTargetInfo;
			const auto lang = info.match([&](
					const Info &data) -> std::optional<Language> {
				for (const auto &language : data.vlanguage_packs().v) {
					const auto parsed = ParseLanguage(language);
					if (parsed.id == langPackId) {
						return parsed;
					}
				}
				return std::nullopt;
			});
			if (lang) {
				performSwitch(*lang);
				requestLangPackStrings(lang->id);
			} else {
				// TLsynchronizeLanguagePack
				requestSyncLangPack(Pack::Base);
				requestSyncLangPack(Pack::Current);
			}
		}).send();
#endif

		return true;
	} else if (result.vname().v == u"suggested_language_pack_id"_q) {
		const auto suggested = Tdb::OptionValueMaybe<QString>(
			result.vvalue());
		setSuggestedLanguage(suggested.value_or(QString()));
		return true;
	}
	return false;
}

void CloudManager::requestLanguageList() {
#if 0 // goodToRemove
	if (!_api) {
		_languagesRequestId = -1;
		return;
	}
	_api->request(base::take(_languagesRequestId)).cancel();
	_languagesRequestId = _api->request(MTPlangpack_GetLanguages(
		MTP_string(CloudLangPackName())
	)).done([=](const MTPVector<MTPLangPackLanguage> &result) {
		auto languages = Languages();
		for (const auto &language : result.v) {
			languages.push_back(ParseLanguage(language));
		}
		if (_languages != languages) {
			_languages = languages;
			_languageListChanged.fire({});
		}
		_languagesRequestId = 0;
	}).fail([=] {
		_languagesRequestId = 0;
	}).send();
#endif
	if (!_api) {
		_languagesRequestId = -1;
		return;
	}
	_languagesRequestId = _api->request(Tdb::TLgetLocalizationTargetInfo(
		Tdb::tl_bool(false)
	)).done([=](const Tdb::TLDlocalizationTargetInfo &data) {
		auto languages = Languages();
		for (const auto &language : data.vlanguage_packs().v) {
			languages.push_back(ParseLanguage(language));
		}
		if (_languages != languages) {
			_languages = languages;
			_languageListChanged.fire({});
		}
		_languagesRequestId = 0;
	}).fail([=](const Tdb::Error &error) {
		_languagesRequestId = 0;
	}).send();
}

void CloudManager::offerSwitchLangPack() {
	Expects(!_offerSwitchToId.isEmpty());
	Expects(_offerSwitchToId != DefaultLanguageId());

	if (!showOfferSwitchBox()) {
		languageListChanged(
		) | rpl::start_with_next([=] {
			showOfferSwitchBox();
		}, _lifetime);
		requestLanguageList();
	}
}

Language CloudManager::findOfferedLanguage() const {
	for (const auto &language : _languages) {
		if (language.id == _offerSwitchToId) {
			return language;
		}
	}
	return {};
}

bool CloudManager::showOfferSwitchBox() {
	const auto language = findOfferedLanguage();
	if (language.id.isEmpty()) {
		return false;
	}

	const auto confirm = [=] {
		Ui::hideLayer();
		if (_offerSwitchToId.isEmpty()) {
			return;
		}
		performSwitchAndRestart(language);
	};
	const auto cancel = [=] {
		Ui::hideLayer();
		changeIdAndReInitConnection(DefaultLanguage());
		Local::writeLangPack();
	};
	Ui::show(
		Ui::MakeConfirmBox({
			.text = QString("Do you want to switch your language to ")
			+ language.nativeName
			+ QString("? You can always change your language in Settings."),
			.confirmed = confirm,
			.cancelled = cancel,
			.confirmText = QString("Change"),
		}),
		Ui::LayerOption::KeepOther);
	return true;
}

#if 0 // goodToRemove
void CloudManager::applyLangPackData(
		Pack pack,
		const MTPDlangPackDifference &data) {
	if (_langpack.version(pack) < data.vfrom_version().v) {
		requestLangPackDifference(pack);
	} else if (!data.vstrings().v.isEmpty()) {
		_langpack.applyDifference(pack, data);
		Local::writeLangPack();
	} else if (_restartAfterSwitch) {
		Local::writeLangPack();
	} else {
		LOG(("Lang Info: Up to date."));
	}
}
#endif

bool CloudManager::canApplyWithoutRestart(const QString &id) const {
	if (id == u"#TEST_X"_q || id == u"#TEST_0"_q) {
		return true;
	}
	return Core::App().canApplyLangPackWithoutRestart();
}

void CloudManager::resetToDefault() {
	performSwitch(DefaultLanguage());
}

void CloudManager::switchToLanguage(const QString &id) {
	requestLanguageAndSwitch(id, false);
}

void CloudManager::switchWithWarning(const QString &id) {
	requestLanguageAndSwitch(id, true);
}

void CloudManager::requestLanguageAndSwitch(
		const QString &id,
		bool warning) {
	Expects(!id.isEmpty());

	if (LanguageIdOrDefault(_langpack.id()) == id) {
		Ui::show(Ui::MakeInformBox(tr::lng_language_already()));
		return;
	} else if (id == u"#custom"_q) {
		performSwitchToCustom();
		return;
	}

	_switchingToLanguageId = id;
	_switchingToLanguageWarning = warning;
	sendSwitchingToLanguageRequest();
}

void CloudManager::sendSwitchingToLanguageRequest() {
#if 0 // goodToRemove
	if (!_api) {
		_switchingToLanguageRequest = -1;
		return;
	}
	_api->request(_switchingToLanguageRequest).cancel();
	_switchingToLanguageRequest = _api->request(MTPlangpack_GetLanguage(
		MTP_string(Lang::CloudLangPackName()),
		MTP_string(_switchingToLanguageId)
	)).done([=](const MTPLangPackLanguage &result) {
		_switchingToLanguageRequest = 0;
		const auto language = Lang::ParseLanguage(result);
		const auto finalize = [=] {
			if (canApplyWithoutRestart(language.id)) {
				performSwitchAndAddToRecent(language);
			} else {
				performSwitchAndRestart(language);
			}
		};
		if (!_switchingToLanguageWarning) {
			finalize();
			return;
		}
		result.match([=](const MTPDlangPackLanguage &data) {
			if (data.vstrings_count().v > 0) {
				Ui::show(Box<ConfirmSwitchBox>(data, finalize));
			} else {
				Ui::show(Box<NotReadyBox>(data));
			}
		});
	}).fail([=](const MTP::Error &error) {
		_switchingToLanguageRequest = 0;
		if (error.type() == "LANG_CODE_NOT_SUPPORTED") {
			Ui::show(Ui::MakeInformBox(tr::lng_language_not_found()));
		}
	}).send();
#endif
	if (!_api) {
		_switchingToLanguageId = -1;
		return;
	}
	_switchingToLanguageRequest = _api->request(Tdb::TLgetLanguagePackInfo(
		Tdb::tl_string(_switchingToLanguageId)
	)).done([=](const Tdb::TLlanguagePackInfo &result) {
		_switchingToLanguageRequest = 0;
		const auto language = Lang::ParseLanguage(result);
		const auto finalize = [=] {
			if (canApplyWithoutRestart(language.id)) {
				performSwitchAndAddToRecent(language);
			} else {
				performSwitchAndRestart(language);
			}
		};
		if (!_switchingToLanguageWarning) {
			finalize();
			return;
		}
		result.match([=](const Tdb::TLDlanguagePackInfo &data) {
			if (data.vtotal_string_count().v > 0) {
				Ui::show(Box<ConfirmSwitchBox>(data, finalize));
			} else {
				Ui::show(Box<NotReadyBox>(data));
			}
		});
	}).fail([=](const Tdb::Error &error) {
		_switchingToLanguageRequest = 0;
		if (error.message == "LANG_CODE_NOT_SUPPORTED") {
			Ui::show(Ui::MakeInformBox(tr::lng_language_not_found(tr::now)));
		}
	}).send();
}

void CloudManager::switchToLanguage(const Language &data) {
	if (_langpack.id() == data.id && data.id != u"#custom"_q) {
		return;
#if 0 // goodToRemove
	} else if (!_api) {
#endif
	} else if (!_api) {
		return;
	}

#if 0 // goodToRemove
	_api->request(base::take(_getKeysForSwitchRequestId)).cancel();
#endif
	if (data.id == u"#custom"_q) {
		performSwitchToCustom();
	} else if (canApplyWithoutRestart(data.id)) {
		performSwitchAndAddToRecent(data);
	} else {
#if 0 // goodToRemove
		QVector<MTPstring> keys;
		keys.reserve(3);
		keys.push_back(MTP_string("lng_sure_save_language"));
		_getKeysForSwitchRequestId = _api->request(MTPlangpack_GetStrings(
			MTP_string(Lang::CloudLangPackName()),
			MTP_string(data.id),
			MTP_vector<MTPstring>(std::move(keys))
		)).done([=](const MTPVector<MTPLangPackString> &result) {
#endif
		using namespace Tdb;
		_getKeysForSwitchRequestId = _api->request(
			TLgetLanguagePackStrings(
				tl_string(data.id),
				tl_vector<TLstring>(1, tl_string("lng_sure_save_language"))
		)).done([=](const TLlanguagePackStrings &result) {
			_getKeysForSwitchRequestId = 0;
			const auto values = Instance::ParseStrings(result);
			const auto getValue = [&](ushort key) {
				auto it = values.find(key);
				return (it == values.cend())
					? GetOriginalValue(key)
					: it->second;
			};
			const auto text = tr::lng_sure_save_language(tr::now)
				+ "\n\n"
				+ getValue(tr::lng_sure_save_language.base);
			Ui::show(
				Ui::MakeConfirmBox({
					.text = text,
					.confirmed = [=] { performSwitchAndRestart(data); },
					.confirmText = tr::lng_box_ok(),
				}),
				Ui::LayerOption::KeepOther);
		}).fail([=] {
			_getKeysForSwitchRequestId = 0;
		}).send();
	}
}

void CloudManager::performSwitchToCustom() {
	auto filter = u"Language files (*.strings)"_q;
	auto title = u"Choose language .strings file"_q;
	FileDialog::GetOpenPath(Core::App().getFileDialogParent(), title, filter, [=, weak = base::make_weak(this)](const FileDialog::OpenResult &result) {
		if (!weak || result.paths.isEmpty()) {
			return;
		}

		const auto filePath = result.paths.front();
		auto loader = Lang::FileParser(
			filePath,
			{ tr::lng_sure_save_language.base });
		if (loader.errors().isEmpty()) {
#if 0 // goodToRemove
			if (_api) {
				_api->request(
					base::take(_switchingToLanguageRequest)
				).cancel();
			}
#endif
			if (canApplyWithoutRestart(u"#custom"_q)) {
				_langpack.switchToCustomFile(filePath);
			} else {
				const auto values = loader.found();
				const auto getValue = [&](ushort key) {
					const auto it = values.find(key);
					return (it == values.cend())
						? GetOriginalValue(key)
						: it.value();
				};
				const auto text = tr::lng_sure_save_language(tr::now)
					+ "\n\n"
					+ getValue(tr::lng_sure_save_language.base);
				const auto change = [=] {
					_langpack.switchToCustomFile(filePath);
					Core::Restart();
				};
				Ui::show(
					Ui::MakeConfirmBox({
						.text = text,
						.confirmed = change,
						.confirmText = tr::lng_box_ok(),
					}),
					Ui::LayerOption::KeepOther);
			}
		} else {
			Ui::show(
				Ui::MakeInformBox(
					"Custom lang failed :(\n\nError: " + loader.errors()),
				Ui::LayerOption::KeepOther);
		}
	});
}

void CloudManager::switchToTestLanguage() {
	const auto testLanguageId = (_langpack.id() == u"#TEST_X"_q)
		? u"#TEST_0"_q
		: u"#TEST_X"_q;
	performSwitch({ testLanguageId });
}

void CloudManager::performSwitch(const Language &data) {
	_restartAfterSwitch = false;
	switchLangPackId(data);
	_api->request(Tdb::TLsetOption(
		Tdb::tl_string("language_pack_id"),
		Tdb::tl_optionValueString(Tdb::tl_string(data.id))
	)).send();
#if 0 // goodToRemove (Should be moved to update applying.)
	requestLangPackDifference(Pack::Current);
	requestLangPackDifference(Pack::Base);
#endif
	requestLangPackStrings(Pack::Current);
	requestLangPackStrings(Pack::Base);
}

void CloudManager::performSwitchAndAddToRecent(const Language &data) {
	Local::pushRecentLanguage(data);
	performSwitch(data);
}

void CloudManager::performSwitchAndRestart(const Language &data) {
	performSwitchAndAddToRecent(data);
	restartAfterSwitch();
}

void CloudManager::restartAfterSwitch() {
	if (_langPackRequestId || _langPackBaseRequestId) {
		_restartAfterSwitch = true;
	} else {
		Core::Restart();
	}
}

void CloudManager::switchLangPackId(const Language &data) {
	const auto currentId = _langpack.id();
	const auto currentBaseId = _langpack.baseId();
	const auto notChanged = (currentId == data.id
		&& currentBaseId == data.baseId)
		|| (currentId.isEmpty()
			&& currentBaseId.isEmpty()
			&& data.id == DefaultLanguageId());
	if (!notChanged) {
		changeIdAndReInitConnection(data);
	}
}

void CloudManager::changeIdAndReInitConnection(const Language &data) {
	_langpack.switchToId(data);
#if 0 // doLater - TDLib connection
	if (_api) {
		const auto mtproto = &_api->instance();
		mtproto->reInitConnection(mtproto->mainDcId());
	}
#endif
}

void CloudManager::resendRequests() {
#if 0 // goodToRemove
	if (packRequestId(Pack::Base)) {
		requestLangPackDifference(Pack::Base);
	}
	if (packRequestId(Pack::Current)) {
		requestLangPackDifference(Pack::Current);
	}
#endif
	for (const auto &pack : { Pack::Base, Pack::Current }) {
		if (packRequestId(pack)) {
			requestLangPackStrings(pack);
		}
	}
	if (_languagesRequestId) {
		requestLanguageList();
	}
	if (_switchingToLanguageRequest) {
		sendSwitchingToLanguageRequest();
	}
}

CloudManager &CurrentCloudManager() {
	auto result = Core::App().langCloudManager();
	Assert(result != nullptr);
	return *result;
}

} // namespace Lang
