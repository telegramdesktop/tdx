/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/tdb_file_proxy.h"

namespace Tdb::details {

#ifdef Q_OS_WIN

FileProxyImpl::FileProxyImpl(const QString &path) {
	_handle = CreateFile(
		reinterpret_cast<const wchar_t*>(path.utf16()),
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
}

bool FileProxyImpl::valid() const {
	return (_handle != INVALID_HANDLE_VALUE);
}

bool FileProxyImpl::seek(int offset) {
	const auto result = SetFilePointer(_handle, offset, nullptr, FILE_BEGIN);
	return (result != INVALID_SET_FILE_POINTER);
}

QByteArray FileProxyImpl::read(int limit) {
	auto result = QByteArray(limit, Qt::Uninitialized);
	auto read = DWORD(0);
	ReadFile(_handle, result.data(), DWORD(limit), &read, nullptr);
	return (read == limit) ? result : QByteArray();
}

#else // Q_OS_WIN

FileProxyImpl::FileProxyImpl(const QString &path) : _file(path) {
	_file.open(QIODevice::ReadOnly);
}

bool FileProxyImpl::valid() const {
	return _file.isOpen();
}

bool FileProxyImpl::seek(int offset) {
	return _file.seek(offset);
}

QByteArray FileProxyImpl::read(int limit) {
	return _file.read(limit);
}

#endif // Q_OS_WIN

} // namespace Tdb::details

namespace Tdb {

std::unique_ptr<FileProxy> FileProxy::Create(const QString &path) {
	auto result = std::make_unique<FileProxy>(path);
	if (!result->valid()) {
		return nullptr;
	}
	return result;
}

} // namespace Tdb
