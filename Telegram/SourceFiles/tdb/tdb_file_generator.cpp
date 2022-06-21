/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/tdb_file_generator.h"

#include "base/random.h"
#include "tdb/tdb_account.h"

namespace Tdb {
namespace {

constexpr auto kChunk = 128 * 1024;

} // namespace

FileGenerator::FileGenerator(
	not_null<Tdb::Account*> account,
	QByteArray content)
: _account(account)
, _conversion(u"bytes_"_q + QString::number(base::RandomValue<uint32>()))
, _content(std::move(content))
, _api(&account->sender()) {
	_account->registerFileGenerator(this);
}

bool FileGenerator::updateGeneration(int64 id) {
	if (_generationId == id) {
		return false;
	} else if (_generationId) {
		_account->unregisterFileGeneration(_generationId, this);
	}
	_generationId = id;
	if (_generationId) {
		_account->registerFileGeneration(_generationId, this);
	}
	return true;
}

void FileGenerator::start(int64 id) {
	if (!updateGeneration(id)) {
		return;
	}
	const auto writePart = [=](const auto &repeat) -> void {
		_api.request(TLwriteGeneratedFilePart(
			tl_int64(_generationId),
			tl_int53(_generationOffset),
			tl_bytes(_content.mid(
				_generationOffset,
				std::min(kChunk, int(_content.size()) - _generationOffset)))
		)).done([=] {
			_generationOffset += kChunk;
			if (_generationOffset >= _content.size()) {
				finish();
			} else if (_generationId) {
				repeat(repeat);
			}
		}).fail([=](const Error &error) {
			cancel(error.code, error.message);
		}).send();
	};
	writePart(writePart);
}

void FileGenerator::finish() {
	if (const auto id = _generationId) {
		updateGeneration(0);
		_api.request(TLfinishFileGeneration(
			tl_int64(id),
			std::nullopt
		)).send();
	}
}

QString FileGenerator::conversion() const {
	return _conversion;
}

void FileGenerator::cancel() {
	cancel(0, "cancel");
}

void FileGenerator::cancel(int code, const QString &message) {
	if (const auto id = _generationId) {
		updateGeneration(0);
		_api.request(TLfinishFileGeneration(
			tl_int64(id),
			tl_error(tl_int32(code), tl_string(message))
		)).send();
	}
}

TLinputFile FileGenerator::inputFile() const {
	return tl_inputFileGenerated(
		tl_string(),
		tl_string(_conversion),
		tl_int53(_content.size()));
}

rpl::lifetime &FileGenerator::lifetime() {
	return _lifetime;
}

FileGenerator::~FileGenerator() {
	_account->unregisterFileGenerator(this);

	_lifetime.destroy();
	cancel();
}

} // namespace Tdb
