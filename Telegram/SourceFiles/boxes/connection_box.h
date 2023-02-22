/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "base/object_ptr.h"
#include "core/core_settings_proxy.h"
#include "mtproto/connection_abstract.h"
#include "mtproto/mtproto_proxy_data.h"

#include "tdb/tdb_sender.h"

namespace Tdb {
class TLproxy;
class TLproxyType;
} // namespace Tdb

namespace Ui {
class Show;
class BoxContent;
class InputField;
class PortInput;
class PasswordInput;
class Checkbox;
template <typename Enum>
class RadioenumGroup;
template <typename Enum>
class Radioenum;
} // namespace Ui

namespace Main {
class Account;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

class ProxiesBoxController {
public:
	using ProxyData = MTP::ProxyData;
	using Type = ProxyData::Type;

	explicit ProxiesBoxController(not_null<Main::Account*> account);

	static void ShowApplyConfirmation(
		Window::SessionController *controller,
		Type type,
		const QMap<QString, QString> &fields);

	static object_ptr<Ui::BoxContent> CreateOwningBox(
		not_null<Main::Account*> account);
	object_ptr<Ui::BoxContent> create();

	enum class ItemState {
		Connecting,
		Online,
		Checking,
		Available,
		Unavailable
	};
	struct ItemView {
		int id = 0;
		QString type;
		QString host;
		uint32 port = 0;
		int ping = 0;
		bool selected = false;
		bool deleted = false;
		bool supportsShare = false;
		bool supportsCalls = false;
		ItemState state = ItemState::Checking;

	};

	void deleteItem(int id);
	void restoreItem(int id);
	void shareItem(int id);
	void applyItem(int id);
	object_ptr<Ui::BoxContent> editItemBox(int id);
	object_ptr<Ui::BoxContent> addNewItemBox();
	bool setProxySettings(ProxyData::Settings value);
	void setProxyForCalls(bool enabled);
	void setTryIPv6(bool enabled);
	rpl::producer<ProxyData::Settings> proxySettingsValue() const;

	[[nodiscard]] bool contains(const ProxyData &proxy) const;
	void addNewItem(const ProxyData &proxy);

	rpl::producer<ItemView> views() const;

	~ProxiesBoxController();

private:
	using Checker = MTP::details::ConnectionPointer;
	struct Item {
		int id = 0;
		ProxyData data;
		bool deleted = false;
#if 0 // mtp
		Checker checker;
		Checker checkerv6;
#endif
		int tdbId = 0;
		mtpRequestId addRequestId = 0;
		mtpRequestId checkRequestId = 0;
		ItemState state = ItemState::Checking;
		int ping = 0;

	};

	std::vector<Item>::iterator findById(int id);
	std::vector<Item>::iterator findByProxy(const ProxyData &proxy);
	void setDeleted(int id, bool deleted);
	void updateView(const Item &item);
	void share(const ProxyData &proxy);
	void saveDelayed();
	void refreshChecker(Item &item);
	void setupChecker(int id, const Checker &checker);

	void addToTdb(Item &item);
	void resolveTdb();
	void replaceItemWith(
		std::vector<Item>::iterator which,
		std::vector<Item>::iterator with);
	void replaceItemValue(
		std::vector<Item>::iterator which,
		const ProxyData &proxy);

	const not_null<Main::Account*> _account;
	Core::SettingsProxy &_settings;
	Tdb::Sender _sender;
	int _idCounter = 0;
	std::vector<Item> _list;
	rpl::event_stream<ItemView> _views;
	base::Timer _saveTimer;
	rpl::event_stream<ProxyData::Settings> _proxySettingsChanges;
	std::shared_ptr<Ui::Show> _show;

	ProxyData _lastSelectedProxy;
	bool _lastSelectedProxyUsed = false;

	rpl::lifetime _lifetime;

	mtpRequestId _listRequestId = 0;

};

[[nodiscard]] MTP::ProxyData FromTL(const Tdb::TLproxy &value);
[[nodiscard]] Tdb::TLproxyType TypeToTL(const MTP::ProxyData &value);
