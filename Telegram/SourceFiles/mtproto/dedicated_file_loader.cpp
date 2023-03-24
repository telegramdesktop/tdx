/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mtproto/dedicated_file_loader.h"

#include "mtproto/facade.h"
#include "main/main_account.h" // Account::sessionChanges.
#include "main/main_session.h" // Session::account.
#include "core/application.h"
#include "base/call_delayed.h"

#include "tdb/tdb_sender.h"
#include "tdb/tdb_tl_scheme.h"
#include "tdb/tdb_account.h"

namespace MTP {
namespace {

#if 0 // mtp
std::optional<MTPInputChannel> ExtractChannel(
		const MTPcontacts_ResolvedPeer &result) {
	const auto &data = result.c_contacts_resolvedPeer();
	if (const auto peer = peerFromMTP(data.vpeer())) {
		for (const auto &chat : data.vchats().v) {
			if (chat.type() == mtpc_channel) {
				const auto &channel = chat.c_channel();
				if (peer == peerFromChannel(channel.vid())) {
					return MTP_inputChannel(
						channel.vid(),
						MTP_long(channel.vaccess_hash().value_or_empty()));
				}
			}
		}
	}
	return std::nullopt;
}

std::optional<DedicatedLoader::File> ParseFile(
		const MTPmessages_Messages &result) {
	const auto message = GetMessagesElement(result);
	if (!message || message->type() != mtpc_message) {
		LOG(("Update Error: MTP file message not found."));
		return std::nullopt;
	}
	const auto &data = message->c_message();
	const auto media = data.vmedia();
	if (!media || media->type() != mtpc_messageMediaDocument) {
		LOG(("Update Error: MTP file media not found."));
		return std::nullopt;
	}
	const auto &inner = media->c_messageMediaDocument();
	const auto document = inner.vdocument();
	if (!document || document->type() != mtpc_document) {
		LOG(("Update Error: MTP file not found."));
		return std::nullopt;
	}
	const auto &fields = document->c_document();
	const auto name = [&] {
		for (const auto &attribute : fields.vattributes().v) {
			if (attribute.type() == mtpc_documentAttributeFilename) {
				const auto &data = attribute.c_documentAttributeFilename();
				return qs(data.vfile_name());
			}
		}
		return QString();
	}();
	if (name.isEmpty()) {
		LOG(("Update Error: MTP file name not found."));
		return std::nullopt;
	}
	const auto size = int64(fields.vsize().v);
	if (size <= 0) {
		LOG(("Update Error: MTP file size is invalid."));
		return std::nullopt;
	}
	const auto location = MTP_inputDocumentFileLocation(
		fields.vid(),
		fields.vaccess_hash(),
		fields.vfile_reference(),
		MTP_string());
	return DedicatedLoader::File{ name, size, fields.vdc_id().v, location };
}
#endif

std::optional<DedicatedLoader::File> ParseFile(
		const Tdb::TLmessageLinkInfo &result) {
	using namespace Tdb;

	struct Fields {
		const TLfile *file = nullptr;
		const TLstring *name = nullptr;
	};

	const auto message = result.data().vmessage();
	if (!message) {
		return std::nullopt;
	}
	auto fields = Fields();
	message->data().vcontent().match([&](
			const TLDmessageAnimation &data) {
		fields = {
			.file = &data.vanimation().data().vanimation(),
			.name = &data.vanimation().data().vfile_name(),
		};
	}, [&](const TLDmessageAudio &data) {
		fields = {
			.file = &data.vaudio().data().vaudio(),
			.name = &data.vaudio().data().vfile_name(),
		};
	}, [&](const TLDmessageDocument &data) {
		fields = {
			.file = &data.vdocument().data().vdocument(),
			.name = &data.vdocument().data().vfile_name(),
		};
	}, [&](const TLDmessageVideo &data) {
		fields = {
			.file = &data.vvideo().data().vvideo(),
			.name = &data.vvideo().data().vfile_name(),
		};
	}, [](const auto &) {});

	if (!fields.file || !fields.name || fields.name->v.isEmpty()) {
		return std::nullopt;
	}
	return DedicatedLoader::File{
		fields.name->v,
		fields.file->data().vsize().v,
		fields.file->data().vid().v,
	};
}

} // namespace

WeakInstance::WeakInstance(base::weak_ptr<Main::Session> session)
: _session(session)
#if 0 // mtp
, _instance(_session ? &_session->account().mtp() : nullptr) {
#endif
, _sender(_session ? &_session->sender() : nullptr) {
	if (!valid()) {
		return;
	}

#if 0 // mtp
	connect(_instance, &QObject::destroyed, this, [=] {
		_instance = nullptr;
		_session = nullptr;
		die();
	});
#endif
	_session->account().sessionChanges(
	) | rpl::filter([](Main::Session *session) {
		return !session;
	}) | rpl::start_with_next([=] {
		die();
		_sender = nullptr;
	}, _lifetime);
}

base::weak_ptr<Main::Session> WeakInstance::session() const {
	return _session;
}

bool WeakInstance::valid() const {
	return (_session != nullptr);
}

#if 0 // mtp
Instance *WeakInstance::instance() const {
	return _instance;
}
#endif

Tdb::Sender *WeakInstance::sender() const {
	return _sender;
}

void WeakInstance::die() {
	for (const auto &[requestId, fail] : base::take(_requests)) {
#if 0 // mtp
		if (_instance) {
			_instance->cancel(requestId);
		}
#endif
		if (_sender) {
			_sender->request(requestId).cancel();
		}
		fail(Error::Local(
#if 0 // mtp
			"UNAVAILABLE",
#endif
			"MTP instance is not available."));
	}
}

bool WeakInstance::removeRequest(mtpRequestId requestId) {
	if (const auto i = _requests.find(requestId); i != end(_requests)) {
		_requests.erase(i);
		return true;
	}
	return false;
}

void WeakInstance::reportUnavailable(
		Fn<void(const Error &error)> callback) {
	InvokeQueued(this, [=] {
		callback(Error::Local(
#if 0 // mtp
			"UNAVAILABLE",
#endif
			"MTP instance is not available."));
	});
}

WeakInstance::~WeakInstance() {
#if 0 // mtp
	if (_instance) {
		for (const auto &[requestId, fail] : base::take(_requests)) {
			_instance->cancel(requestId);
		}
	}
#endif
	if (_sender) {
		for (const auto &[requestId, fail] : base::take(_requests)) {
			_sender->request(requestId).cancel();
		}
	}
}

AbstractDedicatedLoader::AbstractDedicatedLoader(
	const QString &filepath,
	int chunkSize)
: _filepath(filepath)
, _chunkSize(chunkSize) {
}

void AbstractDedicatedLoader::start() {
	if (!validateOutput()
		|| (!_output.isOpen() && !_output.open(QIODevice::Append))) {
		QFile(_filepath).remove();
		threadSafeFailed();
		return;
	}

	LOG(("Update Info: Starting loading '%1' from %2 offset."
		).arg(_filepath
		).arg(alreadySize()));
	startLoading();
}

int64 AbstractDedicatedLoader::alreadySize() const {
	QMutexLocker lock(&_sizesMutex);
	return _alreadySize;
}

int64 AbstractDedicatedLoader::totalSize() const {
	QMutexLocker lock(&_sizesMutex);
	return _totalSize;
}

rpl::producer<QString> AbstractDedicatedLoader::ready() const {
	return _ready.events();
}

auto AbstractDedicatedLoader::progress() const -> rpl::producer<Progress> {
	return _progress.events();
}

rpl::producer<> AbstractDedicatedLoader::failed() const {
	return _failed.events();
}

void AbstractDedicatedLoader::wipeFolder() {
	QFileInfo info(_filepath);
	const auto dir = info.dir();
	const auto all = dir.entryInfoList(QDir::Files);
	for (auto i = all.begin(), e = all.end(); i != e; ++i) {
		if (i->absoluteFilePath() != info.absoluteFilePath()) {
			QFile::remove(i->absoluteFilePath());
		}
	}
}

bool AbstractDedicatedLoader::validateOutput() {
	if (_filepath.isEmpty()) {
		return false;
	}

	QFileInfo info(_filepath);
	const auto dir = info.dir();
	if (!dir.exists()) {
		dir.mkdir(dir.absolutePath());
	}
	_output.setFileName(_filepath);

	if (!info.exists()) {
		return true;
	}
	const auto fullSize = info.size();
	if (fullSize < _chunkSize || fullSize > kMaxFileSize) {
		return _output.remove();
	}
	const auto goodSize = int64((fullSize % _chunkSize)
		? (fullSize - (fullSize % _chunkSize))
		: fullSize);
	if (_output.resize(goodSize)) {
		_alreadySize = goodSize;
		return true;
	}
	return false;
}

void AbstractDedicatedLoader::threadSafeProgress(Progress progress) {
	crl::on_main(this, [=] {
		_progress.fire_copy(progress);
	});
}

void AbstractDedicatedLoader::threadSafeReady() {
	crl::on_main(this, [=] {
		_ready.fire_copy(_filepath);
	});
}

void AbstractDedicatedLoader::threadSafeFailed() {
	crl::on_main(this, [=] {
		_failed.fire({});
	});
}

void AbstractDedicatedLoader::writeChunk(bytes::const_span data, int totalSize) {
	const auto size = data.size();
	if (size > 0) {
		const auto written = _output.write(QByteArray::fromRawData(
			reinterpret_cast<const char*>(data.data()),
			size));
		if (written != size) {
			threadSafeFailed();
			return;
		}
	}

	const auto progress = [&] {
		QMutexLocker lock(&_sizesMutex);
		if (!_totalSize) {
			_totalSize = totalSize;
		}
		_alreadySize += size;
		return Progress { _alreadySize, _totalSize };
	}();

	if (progress.size > 0 && progress.already >= progress.size) {
		_output.close();
		threadSafeReady();
	} else {
		threadSafeProgress(progress);
	}
}

rpl::lifetime &AbstractDedicatedLoader::lifetime() {
	return _lifetime;
}

DedicatedLoader::DedicatedLoader(
	base::weak_ptr<Main::Session> session,
	const QString &folder,
	const File &file)
: AbstractDedicatedLoader(folder + '/' + file.name, kChunkSize)
, _size(file.size)
#if 0 // mtp
, _dcId(file.dcId)
, _location(file.location)
#endif
, _file(file)
, _mtp(session) {
	Expects(_size > 0);
}

void DedicatedLoader::startLoading() {
	if (!_mtp.valid()) {
		LOG(("Update Error: MTP is unavailable."));
		threadSafeFailed();
		return;
	} else if (_downloadLifetime) {
		return;
	}

#if 0 // mtp
	LOG(("Update Info: Loading using MTP from '%1'.").arg(_dcId));
	_offset = alreadySize();
	writeChunk({}, _size);
	sendRequest();
#endif
	writeChunk({}, _size);
	const auto process = [=](const Tdb::TLfile &file) {
		const auto &data = file.data().vlocal().data();
		const auto downloadedOffset = data.vdownload_offset().v;
		const auto downloadedTill = data.vis_downloading_completed().v
			? _size
			: (data.vdownload_offset().v + data.vdownloaded_prefix_size().v);
		if (downloadedTill >= _size) {
			LOG(("Update Info: MTP load finished."));
			_downloadLifetime.destroy();
		} else if (!data.vis_downloading_active().v) {
			LOG(("Update Error: MTP load stopped at %1, size: %2."
				).arg(downloadedTill
				).arg(_size));
			threadSafeFailed();
			return;
		}
		const auto writeRequired = [&] {
			return (_writtenTill < _size)
				&& ((downloadedTill >= _size)
					|| (downloadedTill >= _writtenTill + kChunkSize));
		};
		if (!writeRequired()) {
			return;
		}
		if (!_proxy) {
			_proxy = Tdb::FileProxy::Create(data.vpath().v);
			if (!_proxy) {
				// Maybe the file was moved and we wait for a new updateFile.
				return;
			}
		}
		if (!_proxy->seek(_writtenTill)) {
			LOG(("Update Error: MTP proxy seek failed to %1, size: %2."
				).arg(_writtenTill
				).arg(_size));
			threadSafeFailed();
			return;
		}
		do {
			const auto read = std::min(int(downloadedTill - _writtenTill), kChunkSize);
			const auto bytes = _proxy->read(read);
			if (bytes.size() != read) {
				LOG(("Update Error: MTP proxy read failed: %1 (%2 of %3)."
					).arg(bytes.size()
					).arg(read
					).arg(_size));
				threadSafeFailed();
				return;
			}
			_writtenTill += read;
			writeChunk(bytes::make_span(bytes), _size);
		} while (writeRequired());
	};
	_writtenTill = alreadySize();
	_mtp.send(Tdb::TLdownloadFile(
		Tdb::tl_int32(_file.id),
		Tdb::tl_int32(Tdb::kDefaultDownloadPriority
			- (_file.lowPriority ? 1 : 0)),
		Tdb::tl_int53(_writtenTill),
		Tdb::tl_int53(0),
		Tdb::tl_bool(false)
	), [=](const Tdb::TLfile &result) {
		process(result);
		_downloadLifetime = _mtp.session()->tdb().updates(
		) | rpl::start_with_next([=](const Tdb::TLupdate &update) {
			if (update.type() == Tdb::id_updateFile) {
				const auto &file = update.c_updateFile().vfile();
				if (file.data().vid().v == _file.id) {
					process(file);
				}
			}
		});
	}, [=](const Tdb::Error &error) {
		LOG(("Update Error: MTP load failed with '%1'"
			).arg(QString::number(error.code) + ':' + error.message));
		threadSafeFailed();
	});
}

#if 0 // mtp
void DedicatedLoader::sendRequest() {
	if (_requests.size() >= kRequestsCount || _offset >= _size) {
		return;
	}
	const auto offset = _offset;
	_requests.push_back({ offset });
	_mtp.send(
		MTPupload_GetFile(
			MTP_flags(0),
			_location,
			MTP_long(offset),
			MTP_int(kChunkSize)),
		[=](const MTPupload_File &result) { gotPart(offset, result); },
		failHandler(),
		MTP::updaterDcId(_dcId));
	_offset += kChunkSize;

	if (_requests.size() < kRequestsCount) {
		base::call_delayed(kNextRequestDelay, this, [=] { sendRequest(); });
	}
}

void DedicatedLoader::gotPart(int offset, const MTPupload_File &result) {
	Expects(!_requests.empty());

	if (result.type() == mtpc_upload_fileCdnRedirect) {
		LOG(("Update Error: MTP does not support cdn right now."));
		threadSafeFailed();
		return;
	}
	const auto &data = result.c_upload_file();
	if (data.vbytes().v.isEmpty()) {
		LOG(("Update Error: MTP empty part received."));
		threadSafeFailed();
		return;
	}

	const auto i = ranges::find(
		_requests,
		offset,
		[](const Request &request) { return request.offset; });
	Assert(i != end(_requests));

	i->bytes = data.vbytes().v;
	while (!_requests.empty() && !_requests.front().bytes.isEmpty()) {
		writeChunk(bytes::make_span(_requests.front().bytes), _size);
		_requests.pop_front();
	}
	sendRequest();
}

Fn<void(const Error &)> DedicatedLoader::failHandler() {
	return [=](const Error &error) {
		LOG(("Update Error: MTP load failed with '%1'"
			).arg(QString::number(error.code()) + ':' + error.type()));
		threadSafeFailed();
	};
}
#endif

void ResolveChannel(
		not_null<MTP::WeakInstance*> mtp,
		const QString &username,
#if 0 // mtp
		Fn<void(const MTPInputChannel &channel)> done,
#endif
		Fn<void(ChannelId channel)> done,
		Fn<void()> fail) {
	const auto failed = [&] {
		LOG(("Dedicated MTP Error: Channel '%1' resolve failed."
			).arg(username));
		fail();
	};
	const auto session = mtp->session();
	if (!mtp->valid()) {
		failed();
		return;
	}

	struct ResolveResult {
		base::weak_ptr<Main::Session> session;
		ChannelId channel = 0;
	};
	static base::flat_map<QString, ResolveResult> ResolveCache;
#if 0 // mtp
		MTPInputChannel channel;
	};
	static std::map<QString, ResolveResult> ResolveCache;
#endif

	const auto i = ResolveCache.find(username);
	if (i != end(ResolveCache)) {
		if (i->second.session.get() == session.get()) {
			done(i->second.channel);
			return;
		}
		ResolveCache.erase(i);
	}

	mtp->send(Tdb::TLsearchPublicChat(Tdb::tl_string(username)), [=](
			const Tdb::TLchat &result) {
		const auto peerId = peerFromTdbChat(result.data().vid());
		if (const auto channel = peerToChannel(peerId)) {
			ResolveCache.emplace(
				username,
				ResolveResult { session, channel });
			done(channel);
		} else {
			failed();
		}
	}, [=](const Tdb::Error &error) {
		LOG(("Dedicated MTP Error: Resolve failed with '%1'"
			).arg(QString::number(error.code) + ':' + error.message));
		fail();
	});
#if 0 // mtp
	const auto doneHandler = [=](const MTPcontacts_ResolvedPeer &result) {
		Expects(result.type() == mtpc_contacts_resolvedPeer);

		if (const auto channel = ExtractChannel(result)) {
			ResolveCache.emplace(
				username,
				ResolveResult { session, *channel });
			done(*channel);
		} else {
			failed();
		}
	};
	const auto failHandler = [=](const Error &error) {
		LOG(("Dedicated MTP Error: Resolve failed with '%1'"
			).arg(QString::number(error.code()) + ':' + error.type()));
		fail();
	};
	mtp->send(
		MTPcontacts_ResolveUsername(MTP_string(username)),
		doneHandler,
		failHandler);
#endif
}

#if 0 // mtp
std::optional<MTPMessage> GetMessagesElement(
		const MTPmessages_Messages &list) {
	return list.match([&](const MTPDmessages_messagesNotModified &) {
		return std::optional<MTPMessage>(std::nullopt);
	}, [&](const auto &data) {
		return data.vmessages().v.isEmpty()
			? std::nullopt
			: std::make_optional(data.vmessages().v[0]);
	});
}
#endif

void StartDedicatedLoader(
		not_null<MTP::WeakInstance*> mtp,
		const DedicatedLoader::Location &location,
		const QString &folder,
		Fn<void(std::unique_ptr<DedicatedLoader>)> ready) {
	const auto link = u"https://t.me/"_q
		+ location.username
		+ '/'
		+ QString::number(location.postId);
	const auto lowPriority = location.lowPriority;
	const auto fail = [=](const Tdb::Error &error) {
		LOG(("Update Error: MTP check failed with '%1'"
			).arg(QString::number(error.code) + ':' + error.message));
		ready(nullptr);
	};
	mtp->send(Tdb::TLgetInternalLinkType(Tdb::tl_string(link)), [=](
			const Tdb::TLinternalLinkType &result) {
		result.match([&](const Tdb::TLDinternalLinkTypeMessage &data) {
			mtp->send(Tdb::TLgetMessageLinkInfo(data.vurl()), [=](
					const Tdb::TLmessageLinkInfo &result) {
				if (auto file = ParseFile(result)) {
					file->lowPriority = lowPriority;
					ready(std::make_unique<MTP::DedicatedLoader>(
						mtp->session(),
						folder,
						*file));
				} else {
					LOG(("Update Error: "
						"MTP check failed with no message: %1").arg(link));
					ready(nullptr);
				}
			}, fail);
		}, [&](const auto &other) {
			LOG(("Update Error: "
				"MTP check failed with wrong link type for: %1").arg(link));
			ready(nullptr);
		});
	}, fail);
#if 0 // mtp
	const auto doneHandler = [=](const MTPmessages_Messages &result) {
		const auto file = ParseFile(result);
		ready(file
			? std::make_unique<MTP::DedicatedLoader>(
				mtp->session(),
				folder,
				*file)
			: nullptr);
	};
	const auto failHandler = [=](const Error &error) {
		LOG(("Update Error: MTP check failed with '%1'"
			).arg(QString::number(error.code()) + ':' + error.type()));
		ready(nullptr);
	};

	const auto &[username, postId] = location;
	ResolveChannel(mtp, username, [=, postId = postId](
			const MTPInputChannel &channel) {
		mtp->send(
			MTPchannels_GetMessages(
				channel,
				MTP_vector<MTPInputMessage>(
					1,
					MTP_inputMessageID(MTP_int(postId)))),
			doneHandler,
			failHandler);
	}, [=] { ready(nullptr); });
#endif

}

} // namespace MTP
