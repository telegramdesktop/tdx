/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/image/image_location.h"

namespace Tdb {
class TLphotoSize;
class TLthumbnail;
class TLanimatedChatPhoto;
} // namespace Tdb

namespace Main {
class Session;
} // namespace Main

namespace Images {

[[nodiscard]] ImageWithLocation FromPhotoSize(
	not_null<Main::Session*> session,
	const MTPDphoto &photo,
	const MTPPhotoSize &size);
[[nodiscard]] ImageWithLocation FromProgressiveSize(
	not_null<Main::Session*> session,
	const MTPPhotoSize &size,
	int index);
[[nodiscard]] ImageWithLocation FromPhotoSize(
	not_null<Main::Session*> session,
	const MTPDdocument &document,
	const MTPPhotoSize &size);
[[nodiscard]] ImageWithLocation FromPhotoSize(
	not_null<Main::Session*> session,
	const MTPDstickerSet &set,
	const MTPPhotoSize &size);
[[nodiscard]] ImageWithLocation FromVideoSize(
	not_null<Main::Session*> session,
	const MTPDdocument &document,
	const MTPVideoSize &size);
[[nodiscard]] ImageWithLocation FromVideoSize(
	not_null<Main::Session*> session,
	const MTPDphoto &photo,
	const MTPVideoSize &size);
[[nodiscard]] ImageWithLocation FromImageInMemory(
	const QImage &image,
	const char *format,
	QByteArray bytes = QByteArray());
[[nodiscard]] ImageLocation FromWebDocument(const MTPWebDocument &document);

[[nodiscard]] ImageWithLocation FromPhotoSize(const Tdb::TLphotoSize *size);
[[nodiscard]] ImageWithLocation FromTdbFile(
	const Tdb::TLfile &file,
	int width,
	int height);
[[nodiscard]] ImageWithLocation FromProgressiveSize(
	const Tdb::TLphotoSize &size,
	int index);
[[nodiscard]] ImageWithLocation FromAnimationSize(
	const Tdb::TLanimatedChatPhoto &photo);
[[nodiscard]] ImageWithLocation FromThumbnail(
	const Tdb::TLthumbnail &thumbnail);

} // namespace Images
