/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#ifdef Q_OS_WIN
#include <windows.h>
#else // Q_OS_WIN
#include <QFile>
#endif // Q_OS_WIN

namespace Tdb::details {

#ifdef Q_OS_WIN

// To be able to read file that is being renamed by TDLib we should pass
// FILE_SHARE_DELETE flag when opening it on Windows. Qt never passes this
// flag in QFile, so we provide a custom implementation instead.
class FileProxyImpl final {
public:
	explicit FileProxyImpl(const QString &path);

	[[nodiscard]] bool valid() const;
	[[nodiscard]] bool seek(int offset);
	[[nodiscard]] QByteArray read(int limit);

private:
	HANDLE _handle = nullptr;

};

#else // Q_OS_WIN

// A simple proxy to QFile on other systems.
class FileProxyImpl final {
public:
	explicit FileProxyImpl(const QString &path);

	[[nodiscard]] bool valid() const;
	[[nodiscard]] bool seek(int offset);
	[[nodiscard]] QByteArray read(int limit);

private:
	QFile _file;

};

#endif // Q_OS_WIN

} // namespace Tdb::details

namespace Tdb {

class FileProxy final {
public:
	explicit FileProxy(const QString &path) : _impl(path) {
	}

	[[nodiscard]] bool valid() const {
		return _impl.valid();
	}
	[[nodiscard]] bool seek(int offset) {
		return _impl.seek(offset);
	}
	[[nodiscard]] QByteArray read(int limit) {
		return _impl.read(limit);
	}

	[[nodiscard]] static std::unique_ptr<FileProxy> Create(
		const QString &path);

private:
	details::FileProxyImpl _impl;

};

} // namespace Tdb
