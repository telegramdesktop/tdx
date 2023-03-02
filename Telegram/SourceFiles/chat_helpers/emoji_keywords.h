/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class ApiWrap;

namespace ChatHelpers {
namespace details {

class EmojiKeywordsLangPackDelegate {
public:
	virtual ApiWrap *api() = 0;
	virtual void langPackRefreshed() = 0;

protected:
	~EmojiKeywordsLangPackDelegate() = default;

};

} // namespace details

class EmojiKeywords final : private details::EmojiKeywordsLangPackDelegate {
public:
	EmojiKeywords();
	EmojiKeywords(const EmojiKeywords &other) = delete;
	EmojiKeywords &operator=(const EmojiKeywords &other) = delete;
	~EmojiKeywords();

	void refresh();

#if 0 // mtp
	[[nodiscard]] rpl::producer<> refreshed() const;
#endif

	struct Result {
		EmojiPtr emoji = nullptr;
		QString label;
		QString replacement;
	};
#if 0 // mtp
	[[nodiscard]] std::vector<Result> query(
		const QString &query,
		bool exact = false) const;
	[[nodiscard]] std::vector<Result> queryMine(
		const QString &query,
		bool exact = false) const;
#endif
	mtpRequestId requestMine(
		const QString &query,
		Fn<void(std::vector<Result>)> callback,
		mtpRequestId previousId = 0,
		bool exact = false);
	[[nodiscard]] int maxQueryLength() const;

private:
	class LangPack;

	not_null<details::EmojiKeywordsLangPackDelegate*> delegate();
	ApiWrap *api() override;
	void langPackRefreshed() override;

	[[nodiscard]] static std::vector<Result> PrioritizeRecent(
		std::vector<Result> list);
	[[nodiscard]] static std::vector<Result> ApplyVariants(
		std::vector<Result> list);

	void handleSessionChanges();
	void apiChanged(ApiWrap *api);
	void refreshInputLanguages();
	[[nodiscard]] std::vector<QString> languages();
#if 0 // mtp
	void refreshRemoteList();
	void setRemoteList(std::vector<QString> &&list);
	void refreshFromRemoteList();
#endif

	ApiWrap *_api = nullptr;
	std::vector<QString> _localList;
#if 0 // mtp
	std::vector<QString> _remoteList;
	mtpRequestId _langsRequestId = 0;
	base::flat_map<QString, std::unique_ptr<LangPack>> _data;
	std::deque<std::unique_ptr<LangPack>> _notUsedData;
#endif
	struct Request {
		QString query;
		QString normalizedQuery;
		Fn<void(std::vector<Result>)> callback;
	};
	base::flat_map<mtpRequestId, Request> _requests;
	std::deque<QStringList> _inputLanguages;
	rpl::event_stream<> _refreshed;

	rpl::lifetime _suggestedChangeLifetime;

	rpl::lifetime _lifetime;
	base::has_weak_ptr _guard;

};

} // namespace ChatHelpers
