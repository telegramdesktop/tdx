/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/tdb_sender.h"

namespace Main {
class Domain;
} // namespace Main

namespace Countries {

class Manager final {
public:
	Manager(not_null<Main::Domain*> domain);
	~Manager();

	void read();
	void write() const;

	rpl::lifetime &lifetime();

private:
	void request();

#if 0 // goodToRemove
	std::optional<MTP::Sender> _api;
#endif
	std::optional<Tdb::Sender> _api;
	const QString _path;
	int _hash = 0;

	rpl::lifetime _lifetime;
};

} // namespace Countries
