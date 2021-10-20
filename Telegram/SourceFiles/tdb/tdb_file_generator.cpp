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
: _content(std::move(content))
, _conversion(tl_string((u"raw_%1"_q).arg(base::RandomValue<int>())))
, _api(&account->sender()) {

	const auto writePart = [=](const auto &repeat) -> void {
		_api.request(TLwriteGeneratedFilePart(
			_generationId,
			tl_int32(_generationOffset),
			tl_bytes(_content.mid(
				_generationOffset,
				std::min(kChunk, int(_content.size()) - _generationOffset)))
		)).done([=] {
			_generationOffset += kChunk;
			if (_generationOffset >= _content.size()) {
				finish();
			} else {
				repeat(repeat);
			}
		}).fail([=](const Error &error) {
			_api.request(TLfinishFileGeneration(
				_generationId,
				tl_error(tl_int32(error.code), tl_string(error.message))
			)).send();
		}).send();
	};

	account->updates(
	) | rpl::start_with_next([=](const TLupdate &result) {
		result.match([&](const TLDupdateFileGenerationStart &data) {
			if (data.vconversion() != _conversion) {
				return;
			}
			_generationId = data.vgeneration_id();
			writePart(writePart);
		}, [&](const TLDupdateFileGenerationStop &data) {
			_generationId = tl_int64(0);
			_lifetime.destroy();
		}, [](const auto &) {
		});
	}, _lifetime);
}

void FileGenerator::finish() {
	if (_generationId.v) {
		_api.request(TLfinishFileGeneration(
			_generationId,
			std::nullopt
		)).send();
	}
}

void FileGenerator::cancel() {
	if (_generationId.v) {
		_api.request(TLfinishFileGeneration(
			_generationId,
			tl_error(tl_int32(0), tl_string("cancel"))
		)).send();
	}
}

TLinputFile FileGenerator::inputFile() const {
	return tl_inputFileGenerated(
		tl_string(),
		_conversion,
		tl_int32(_content.size()));
}

rpl::lifetime &FileGenerator::lifetime() {
	return _lifetime;
}

FileGenerator::~FileGenerator() {
	_lifetime.destroy();
	cancel();
}

} // namespace Tdb
