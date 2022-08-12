/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_ringtones.h"

#include "api/api_toggling_media.h"
#include "apiwrap.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "data/notify/data_notify_settings.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "storage/file_upload.h"
#include "storage/localimageloader.h"

#include "tdb/tdb_file_generator.h"
#include "tdb/tdb_tl_scheme.h"

namespace Api {
namespace {

using namespace Tdb;

} // namespace

#if 0 // goodToRemove
namespace {

std::shared_ptr<FilePrepareResult> PrepareRingtoneDocument(
		MTP::DcId dcId,
		const QString &filename,
		const QString &filemime,
		const QByteArray &content) {
	const auto id = base::RandomValue<DocumentId>();
	auto attributes = QVector<MTPDocumentAttribute>(
		1,
		MTP_documentAttributeFilename(MTP_string(filename)));

	auto result = MakePreparedFile({
		.id = id,
		.type = SendMediaType::File,
	});
	result->filename = filename;
	result->content = content;
	result->filesize = content.size();
	result->setFileData(content);
	result->document = MTP_document(
		MTP_flags(0),
		MTP_long(id),
		MTP_long(0),
		MTP_bytes(),
		MTP_int(base::unixtime::now()),
		MTP_string(filemime),
		MTP_long(content.size()),
		MTP_vector<MTPPhotoSize>(),
		MTPVector<MTPVideoSize>(),
		MTP_int(dcId),
		MTP_vector<MTPDocumentAttribute>(std::move(attributes)));
	return result;
}

} // namespace
#endif

Ringtones::Ringtones(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance()) {
#if 0 // goodToRemove
	crl::on_main(_session, [=] {
		// You can't use _session->lifetime() in the constructor,
		// only queued, because it is not constructed yet.
		_session->uploader().documentReady(
		) | rpl::start_with_next([=](const Storage::UploadedMedia &data) {
			ready(data.fullId, data.info.file);
		}, _session->lifetime());
	});
#endif
}

void Ringtones::upload(
		const QString &filename,
		const QString &filemime,
		const QByteArray &content) {
#if 0 // goodToRemove
	const auto ready = PrepareRingtoneDocument(
		_api.instance().mainDcId(),
		filename,
		filemime,
		content);

	const auto uploadedData = UploadedData{ filename, filemime, content };
	const auto fakeId = FullMsgId(
		_session->userPeerId(),
		_session->data().nextLocalMessageId());
	const auto already = ranges::find_if(
		_uploads,
		[&](const auto &d) {
			return uploadedData.filemime == d.second.filemime
				&& uploadedData.filename == d.second.filename;
		});
	if (already != end(_uploads)) {
		_session->uploader().cancel(already->first);
		_uploads.erase(already);
	}
	_uploads.emplace(fakeId, uploadedData);
	_session->uploader().upload(fakeId, ready);
#endif
	const auto token = filename + QString::number(content.size());

	auto generator = std::make_unique<FileGenerator>(
		&_session->tdb(),
		content,
		filename);
	auto inputFile = generator->inputFile();

	const auto eraseExisted = [=] {
		const auto it = _uploads.find(token);
		if (it != end(_uploads)) {
			it->second->cancel();
			_uploads.erase(it);
		}
	};
	eraseExisted();

	generator->lifetime().add(eraseExisted);

	_uploads.emplace(token, std::move(generator));

	_api.request(TLaddSavedNotificationSound(
		std::move(inputFile)
	)).done([=](const TLnotificationSound &result) {
		const auto document = _session->data().processDocument(result);
		_list.documents.insert(_list.documents.begin(), document->id);
		const auto media = document->createMediaView();
		media->setBytes(content);
		document->owner().notifySettings().cacheSound(document);
		_uploadDones.fire_copy(document->id);
	}).fail([=](const Error &error) {
		_uploadFails.fire_copy(error.message);
	}).send();
}

#if 0 // goodToRemove
void Ringtones::ready(const FullMsgId &msgId, const MTPInputFile &file) {
	const auto maybeUploadedData = _uploads.take(msgId);
	if (!maybeUploadedData) {
		return;
	}
	const auto uploadedData = *maybeUploadedData;
	_api.request(MTPaccount_UploadRingtone(
		file,
		MTP_string(uploadedData.filename),
		MTP_string(uploadedData.filemime)
	)).done([=, content = uploadedData.content](const MTPDocument &result) {
		const auto document = _session->data().processDocument(result);
		_list.documents.insert(_list.documents.begin(), document->id);
		const auto media = document->createMediaView();
		media->setBytes(content);
		document->owner().notifySettings().cacheSound(document);
		_uploadDones.fire_copy(document->id);
	}).fail([=](const MTP::Error &error) {
		_uploadFails.fire_copy(error.type());
	}).send();
}
#endif

void Ringtones::requestList() {
	if (_list.requestId) {
		return;
	}
#if 0 // goodToRemove
	_list.requestId = _api.request(
		MTPaccount_GetSavedRingtones(MTP_long(_list.hash))
	).done([=](const MTPaccount_SavedRingtones &result) {
		_list.requestId = 0;
		result.match([&](const MTPDaccount_savedRingtones &data) {
			_list.hash = data.vhash().v;
			_list.documents.clear();
			_list.documents.reserve(data.vringtones().v.size());
			for (const auto &d : data.vringtones().v) {
				const auto document = _session->data().processDocument(d);
				document->forceToCache(true);
				_list.documents.emplace_back(document->id);
			}
			_list.updates.fire({});
		}, [&](const MTPDaccount_savedRingtonesNotModified &) {
		});
#endif
	_list.requestId = _api.request(TLgetSavedNotificationSounds(
	)).done([=](const TLDnotificationSounds &data) {
		_list.requestId = 0;
		_list.documents.clear();
		_list.documents.reserve(data.vnotification_sounds().v.size());
		auto &owner = _session->data();
		for (const auto &s : data.vnotification_sounds().v) {
			const auto document = owner.processDocument(s);
			document->forceToCache(true);
			_list.documents.emplace_back(document->id);
		}
		_list.updates.fire({});
	}).fail([=] {
		_list.requestId = 0;
	}).send();
}

const Ringtones::Ids &Ringtones::list() const {
	return _list.documents;
}

rpl::producer<> Ringtones::listUpdates() const {
	return _list.updates.events();
}

rpl::producer<QString> Ringtones::uploadFails() const {
	return _uploadFails.events();
}

rpl::producer<DocumentId> Ringtones::uploadDones() const {
	return _uploadDones.events();
}

void Ringtones::applyUpdate() {
	_list.hash = 0;
	_list.documents.clear();
	requestList();
}

void Ringtones::remove(DocumentId id) {
	if (const auto document = _session->data().document(id)) {
		ToggleSavedRingtone(
			document,
			Data::FileOriginRingtones(),
			crl::guard(&document->session(), [=] {
				const auto it = ranges::find(_list.documents, id);
				if (it != end(_list.documents)) {
					_list.documents.erase(it);
				}
			}),
			false);
	}
}

int64 Ringtones::maxSize() const {
	return _session->appConfig().get<int>(
		u"ringtone_size_max"_q,
		100 * 1024);
}

int Ringtones::maxSavedCount() const {
	return _session->appConfig().get<int>(
		u"ringtone_saved_count_max"_q,
		100);
}

crl::time Ringtones::maxDuration() const {
	return crl::time(1000) * _session->appConfig().get<int>(
		u"ringtone_duration_max"_q,
		5);
}

} // namespace Api
