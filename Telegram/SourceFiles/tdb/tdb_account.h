/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/details/tdb_instance.h"
#include "tdb/tdb_sender.h"

namespace Tdb {

using AccountConfig = details::InstanceConfig;

class FileGenerator;

class Account final {
public:
	explicit Account(AccountConfig &&config);

	[[nodiscard]] Sender &sender() {
		return _sender;
	}

	[[nodiscard]] rpl::producer<TLupdate> updates() const;
	void logout();

	void registerFileGenerator(not_null<FileGenerator*> generator);
	void unregisterFileGenerator(not_null<FileGenerator*> generator);
	void registerFileGeneration(
		int64 id,
		not_null<FileGenerator*> generator);
	void unregisterFileGeneration(
		int64 id,
		not_null<FileGenerator*> generator);

	[[nodiscard]] rpl::lifetime &lifetime() {
		return _lifetime;
	}

private:
	[[nodiscard]] bool consumeUpdate(const TLupdate &update);

	details::Instance _instance;
	Sender _sender;
	rpl::event_stream<TLupdate> _updates;

	base::flat_map<QString, not_null<FileGenerator*>> _generators;
	base::flat_map<int64, not_null<FileGenerator*>> _generations;

	rpl::lifetime _lifetime;

};

using details::Execute;

[[nodiscard]] inline bool IsRecreatedUpdate(const TLupdate &update) {
	return details::ClientClosedUpdate(update);
}

} // namespace Tdb
