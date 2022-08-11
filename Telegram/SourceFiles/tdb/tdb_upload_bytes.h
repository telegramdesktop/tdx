/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "tdb/tdb_file_generator.h"
#include "tdb/tdb_sender.h"
#include "tdb/tdb_tl_scheme.h"

namespace Tdb {

class Account;

class BytesUploader final {
public:
	using Offset = int;
	using FileId = int;

	BytesUploader(
		not_null<Account*> account,
		QByteArray content,
		const TLfileType &type = tl_fileTypeUnknown());
	~BytesUploader();

	void start();
	void cancel();

	[[nodiscard]] rpl::producer<Offset, QString> updates() const;
	[[nodiscard]] FileId fileId() const;
	[[nodiscard]] rpl::lifetime &lifetime();

private:
	FileGenerator _fileGenerator;
	const TLfileType &_type;
	Sender _api;
	TLint32 _fileId = tl_int32(0);
	rpl::event_stream<Offset, QString> _updates;
	bool _done = false;

	rpl::lifetime _lifetime;

};

} // namespace Tdb
