/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/tdb_sender.h"

namespace Tdb {

class Account;

class FileGenerator final {
public:
	using Offset = int;

	FileGenerator(
		not_null<Account*> account,
		QByteArray content);
	~FileGenerator();

	void cancel();
	void finish();

	[[nodiscard]] TLinputFile inputFile() const;

	[[nodiscard]] rpl::lifetime &lifetime();

private:
	QByteArray _content;
	const TLstring _conversion;
	Sender _api;
	TLint64 _generationId = tl_int64(0);
	Offset _generationOffset = 0;

	rpl::lifetime _lifetime;

};

} // namespace Tdb
