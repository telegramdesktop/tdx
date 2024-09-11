/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_chat_links.h"

#include "api/api_text_entities.h"
#include "apiwrap.h"
#include "data/data_session.h"
#include "main/main_session.h"

#include "tdb/tdb_sender.h"
#include "tdb/tdb_tl_scheme.h"

namespace Api {
namespace {

using namespace Tdb;

#if 0 // mtp
[[nodiscard]] ChatLink FromMTP(
		not_null<Main::Session*> session,
		const MTPBusinessChatLink &link) {
	const auto &data = link.data();
	return {
		.link = qs(data.vlink()),
		.title = qs(data.vtitle().value_or_empty()),
		.message = {
			qs(data.vmessage()),
			EntitiesFromMTP(
				session,
				data.ventities().value_or_empty())
		},
		.clicks = data.vviews().v,
	};
}

[[nodiscard]] MTPInputBusinessChatLink ToMTP(
		not_null<Main::Session*> session,
		const QString &title,
		const TextWithEntities &message) {
	auto entities = EntitiesToMTP(
		session,
		message.entities,
		ConvertOption::SkipLocal);
	using Flag = MTPDinputBusinessChatLink::Flag;
	const auto flags = (title.isEmpty() ? Flag() : Flag::f_title)
		| (entities.v.isEmpty() ? Flag() : Flag::f_entities);
	return MTP_inputBusinessChatLink(
		MTP_flags(flags),
		MTP_string(message.text),
		std::move(entities),
		MTP_string(title));
}
#endif

[[nodiscard]] ChatLink FromTL(const TLbusinessChatLink &link) {
	const auto &data = link.data();
	return {
		.link = data.vlink().v,
		.title = data.vtitle().v,
		.message = FormattedTextFromTdb(data.vtext()),
		.clicks = data.vview_count().v,
	};
}

} // namespace

ChatLinks::ChatLinks(not_null<ApiWrap*> api) : _api(api) {
}

void ChatLinks::create(
		const QString &title,
		const TextWithEntities &message,
		Fn<void(Link)> done) {
	const auto session = &_api->session();
#if 0 // mtp
	_api->request(MTPaccount_CreateBusinessChatLink(
		ToMTP(session, title, message)
	)).done([=](const MTPBusinessChatLink &result) {
		const auto link = FromMTP(session, result);
#endif
	session->sender().request(TLcreateBusinessChatLink(
		tl_inputBusinessChatLink(
			FormattedTextToTdb(message),
			tl_string(title))
	)).done([=](const TLbusinessChatLink &result) {
		const auto link = FromTL(result);
		_list.push_back(link);
		_updates.fire({ .was = QString(), .now = link });
		if (done) done(link);
#if 0 // mtp
	}).fail([=](const MTP::Error &error) {
		const auto type = error.type();
#endif
	}).fail([=] {
		if (done) done(Link());
	}).send();
}

void ChatLinks::edit(
		const QString &link,
		const QString &title,
		const TextWithEntities &message,
		Fn<void(Link)> done) {
	const auto session = &_api->session();
#if 0 // mtp
	_api->request(MTPaccount_EditBusinessChatLink(
		MTP_string(link),
		ToMTP(session, title, message)
	)).done([=](const MTPBusinessChatLink &result) {
		const auto parsed = FromMTP(session, result);
#endif
	session->sender().request(TLeditBusinessChatLink(
		tl_string(link),
		tl_inputBusinessChatLink(
			FormattedTextToTdb(message),
			tl_string(title))
	)).done([=](const TLbusinessChatLink &result) {
		const auto parsed = FromTL(result);
		if (parsed.link != link) {
			LOG(("API Error: EditBusinessChatLink changed the link."));
			if (done) done(Link());
			return;
		}
		const auto i = ranges::find(_list, link, &Link::link);
		if (i != end(_list)) {
			*i = parsed;
			_updates.fire({ .was = link, .now = parsed });
			if (done) done(parsed);
		} else {
			LOG(("API Error: EditBusinessChatLink link not found."));
			if (done) done(Link());
		}
#if 0 // mtp
	}).fail([=](const MTP::Error &error) {
		const auto type = error.type();
#endif
	}).fail([=] {
		if (done) done(Link());
	}).send();
}

void ChatLinks::destroy(
		const QString &link,
		Fn<void()> done) {
#if 0 // mtp
	_api->request(MTPaccount_DeleteBusinessChatLink(
		MTP_string(link)
	)).done([=] {
#endif
	_api->sender().request(TLdeleteBusinessChatLink(
		tl_string(link)
	)).done([=] {
		const auto i = ranges::find(_list, link, &Link::link);
		if (i != end(_list)) {
			_list.erase(i);
			_updates.fire({ .was = link });
			if (done) done();
		} else {
			LOG(("API Error: DeleteBusinessChatLink link not found."));
			if (done) done();
		}
#if 0 // mtp
	}).fail([=](const MTP::Error &error) {
		const auto type = error.type();
#endif
	}).fail([=] {
		if (done) done();
	}).send();
}

void ChatLinks::preload() {
	if (_loaded || _requestId) {
		return;
	}
#if 0 // mtp
	_requestId = _api->request(MTPaccount_GetBusinessChatLinks(
	)).done([=](const MTPaccount_BusinessChatLinks &result) {
		const auto &data = result.data();
		const auto session = &_api->session();
		const auto owner = &session->data();
		owner->processUsers(data.vusers());
		owner->processChats(data.vchats());
#endif
	_requestId = _api->sender().request(TLgetBusinessChatLinks(
	)).done([=](const TLbusinessChatLinks &result) {
		const auto &data = result.data();
		auto links = std::vector<Link>();
		links.reserve(data.vlinks().v.size());
		for (const auto &link : data.vlinks().v) {
#if 0 // mtp
			links.push_back(FromMTP(session, link));
#endif
			links.push_back(FromTL(link));
		}
		_list = std::move(links);
		_loaded = true;
		_loadedUpdates.fire({});
	}).fail([=] {
		_requestId = 0;
		_loaded = true;
		_loadedUpdates.fire({});
	}).send();
}

const std::vector<ChatLink> &ChatLinks::list() const {
	return _list;
}

bool ChatLinks::loaded() const {
	return _loaded;
}

rpl::producer<> ChatLinks::loadedUpdates() const {
	return _loadedUpdates.events();
}

rpl::producer<ChatLinks::Update> ChatLinks::updates() const {
	return _updates.events();
}

} // namespace Api
