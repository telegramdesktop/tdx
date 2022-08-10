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
		QByteArray content,
		QString filename);
	~FileGenerator();

	void start(int64 id);
	void cancel();
	void finish();

	[[nodiscard]] QString conversion() const;
	[[nodiscard]] TLinputFile inputFile() const;

	[[nodiscard]] rpl::lifetime &lifetime();

private:
	bool updateGeneration(int64 id);
	void cancel(int code, const QString &message);

	const not_null<Account*> _account;
	const QString _conversion;

	QByteArray _content;
	QString _filename;
	Sender _api;
	int64 _generationId = 0;
	Offset _generationOffset = 0;

	rpl::lifetime _lifetime;

};

} // namespace Tdb
