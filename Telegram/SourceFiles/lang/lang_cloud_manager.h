/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/sender.h"
#include "base/weak_ptr.h"

namespace Tdb {
class TLDupdateLanguagePackStrings;
class TLlanguagePackString;
class TLlanguagePackInfo;
class TLDupdateOption;
} // namespace Tdb

namespace MTP {
class Instance;
} // namespace MTP

namespace Lang {

class Instance;
enum class Pack;
struct Language;

#if 0 // goodToRemove
Language ParseLanguage(const MTPLangPackLanguage &data);
#endif
Language ParseLanguage(const Tdb::TLlanguagePackInfo &result);

class CloudManager : public base::has_weak_ptr {
public:
	explicit CloudManager(Instance &langpack);

	using Languages = std::vector<Language>;

	void requestLanguageList();
	const Languages &languageList() const {
		return _languages;
	}
	[[nodiscard]] rpl::producer<> languageListChanged() const;
	[[nodiscard]] rpl::producer<> firstLanguageSuggestion() const;
	void requestLangPackStrings(const QString &langId);
#if 0 // goodToRemove
	void requestLangPackDifference(const QString &langId);
	void applyLangPackDifference(const MTPLangPackDifference &difference);
	void setCurrentVersions(int version, int baseVersion);
#endif
	void apply(const Tdb::TLDupdateLanguagePackStrings &result);
	bool apply(const Tdb::TLDupdateOption &result);

	void resetToDefault();
	void switchWithWarning(const QString &id);
	void switchToLanguage(const QString &id);
	void switchToLanguage(const Language &data);
	void switchToTestLanguage();
	void setSuggestedLanguage(const QString &langCode);
	QString suggestedLanguage() const {
		return _suggestedLanguage;
	}

private:
	void init();
	mtpRequestId &packRequestId(Pack pack);
	mtpRequestId packRequestId(Pack pack) const;
	Pack packTypeFromId(const QString &id) const;
	void requestLangPackStrings(Pack pack);
#if 0 // goodToRemove
	void requestLangPackDifference(Pack pack);
#endif
	void applyStrings(
		Pack pack,
		// Instance::TLStrings.
		const QVector<Tdb::TLlanguagePackString> &strings);
	bool canApplyWithoutRestart(const QString &id) const;
	void performSwitchToCustom();
	void performSwitch(const Language &data);
	void performSwitchAndAddToRecent(const Language &data);
	void performSwitchAndRestart(const Language &data);
	void restartAfterSwitch();
	void offerSwitchLangPack();
	bool showOfferSwitchBox();
	Language findOfferedLanguage() const;

	void requestLanguageAndSwitch(const QString &id, bool warning);
#if 0 // goodToRemove
	void applyLangPackData(Pack pack, const MTPDlangPackDifference &data);
#endif
	void switchLangPackId(const Language &data);
	void changeIdAndReInitConnection(const Language &data);

	void sendSwitchingToLanguageRequest();
	void resendRequests();

	std::optional<MTP::Sender> _api;
	Instance &_langpack;
	Languages _languages;
	mtpRequestId _langPackRequestId = 0;
	mtpRequestId _langPackBaseRequestId = 0;
	mtpRequestId _languagesRequestId = 0;

	QString _offerSwitchToId;
	bool _restartAfterSwitch = false;

	QString _suggestedLanguage;
	bool _languageWasSuggested = false;

	mtpRequestId _switchingToLanguageRequest = 0;
	QString _switchingToLanguageId;
	bool _switchingToLanguageWarning = false;

	mtpRequestId _getKeysForSwitchRequestId = 0;

	rpl::event_stream<> _languageListChanged;
	rpl::event_stream<> _firstLanguageSuggestion;

	rpl::lifetime _lifetime;

};

CloudManager &CurrentCloudManager();

} // namespace Lang
