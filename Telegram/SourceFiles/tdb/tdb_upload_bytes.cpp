/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/tdb_upload_bytes.h"

#include "tdb/tdb_account.h"
#include "tdb/tdb_tl_scheme.h"

namespace Tdb {

BytesUploader::BytesUploader(
	not_null<Tdb::Account*> account,
	QByteArray content,
	const TLfileType &type)
: _fileGenerator(account, std::move(content), QString())
, _type(type)
, _api(&account->sender()) {

	account->updates(
	) | rpl::start_with_next([=](const TLupdate &result) {
		result.match([&](const TLDupdateFile &data) {
			const auto &file = data.vfile().data();
			if (file.vid() != _fileId) {
				return;
			}

			const auto &remote = file.vremote().data();
			const auto &local = file.vlocal().data();

			if ((remote.vuploaded_size() == file.vexpected_size())
				&& (local.vdownloaded_size() == file.vexpected_size())) {
				_updates.fire_done();
				_lifetime.destroy();
				_done = true;
				return;
			} else {
				_updates.fire_copy(remote.vuploaded_size().v);
			}
		}, [](const auto &) {
		});
	}, _lifetime);
}

void BytesUploader::start() {
	_api.request(TLuploadFile(
		_fileGenerator.inputFile(),
		_type,
		tl_int32(1)
	)).done([=](const TLDfile &file) {
		_fileId = file.vid();
	}).fail([=](const Error &error) {
		_updates.fire_error_copy(error.message);
	}).send();
}

void BytesUploader::cancel() {
	if (fileId()) {
		_api.request(TLcancelUploadFile(_fileId)).send();
	}
}

auto BytesUploader::updates() const
-> rpl::producer<BytesUploader::Offset, QString> {
	return _updates.events();
}

BytesUploader::FileId BytesUploader::fileId() const {
	return _fileId.v;
}

rpl::lifetime &BytesUploader::lifetime() {
	return _lifetime;
}

BytesUploader::~BytesUploader() {
	_lifetime.destroy();
	if (!_done && fileId()) {
		cancel();
	}
}

} // namespace Tdb
