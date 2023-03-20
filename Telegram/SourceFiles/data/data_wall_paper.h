/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/flags.h"

class Image;

namespace Tdb {
class TLbackground;
class TLbackgroundFill;
class TLbackgroundType;
} // namespace Tdb

namespace Main {
class Session;
} // namespace Main

namespace Ui {

[[nodiscard]] QColor ColorFromSerialized(MTPint serialized);
[[nodiscard]] std::optional<QColor> MaybeColorFromSerialized(
	const tl::conditional<MTPint> &mtp);

[[nodiscard]] std::vector<QColor> ColorsFromFill(
	const Tdb::TLbackgroundFill &fill,
	bool invertTopBottom = false);
[[nodiscard]] Tdb::TLbackgroundFill ColorsToFill(
	const std::vector<QColor> &colors);

} // namespace Ui

namespace Data {

struct FileOrigin;

enum class WallPaperFlag {
	Pattern = (1 << 0),
	Default = (1 << 1),
	Creator = (1 << 2),
	Dark = (1 << 3),
};
inline constexpr bool is_flag_type(WallPaperFlag) { return true; };
using WallPaperFlags = base::flags<WallPaperFlag>;

class WallPaper {
public:
	explicit WallPaper(WallPaperId id);

	void setLocalImageAsThumbnail(std::shared_ptr<Image> image);

	[[nodiscard]] bool equals(const WallPaper &paper) const;

	[[nodiscard]] WallPaperId id() const;
	[[nodiscard]] QString emojiId() const;
	[[nodiscard]] bool isNull() const;
	[[nodiscard]] QString key() const;
	[[nodiscard]] const std::vector<QColor> backgroundColors() const;
	[[nodiscard]] DocumentData *document() const;
	[[nodiscard]] Image *localThumbnail() const;
	[[nodiscard]] bool isPattern() const;
	[[nodiscard]] bool isDefault() const;
#if 0 // mtp
	[[nodiscard]] bool isCreator() const;
#endif
	[[nodiscard]] bool isDark() const;
	[[nodiscard]] bool isLocal() const;
	[[nodiscard]] bool isBlurred() const;
	[[nodiscard]] int patternIntensity() const;
	[[nodiscard]] float64 patternOpacity() const;
	[[nodiscard]] int gradientRotation() const;
	[[nodiscard]] bool hasShareUrl() const;
#if 0 // mtp
	[[nodiscard]] QString shareUrl(not_null<Main::Session*> session) const;
#endif
	[[nodiscard]] Tdb::TLbackgroundType tlType() const;
	void requestShareUrl(
		not_null<Main::Session*> session,
		Fn<void(QString)> done) const;

	void loadDocument() const;
	void loadDocumentThumbnail() const;
	[[nodiscard]] FileOrigin fileOrigin() const;

	[[nodiscard]] UserId ownerId() const;
	[[nodiscard]] MTPInputWallPaper mtpInput(
		not_null<Main::Session*> session) const;
	[[nodiscard]] MTPWallPaperSettings mtpSettings() const;

	[[nodiscard]] WallPaper withUrlParams(
		const QMap<QString, QString> &params) const;
	[[nodiscard]] WallPaper withBlurred(bool blurred) const;
	[[nodiscard]] WallPaper withPatternIntensity(int intensity) const;
	[[nodiscard]] WallPaper withGradientRotation(int rotation) const;
	[[nodiscard]] WallPaper withBackgroundColors(
		std::vector<QColor> colors) const;
	[[nodiscard]] WallPaper withParamsFrom(const WallPaper &other) const;
	[[nodiscard]] WallPaper withoutImageData() const;

#if 0 // mtp
	[[nodiscard]] static std::optional<WallPaper> Create(
		not_null<Main::Session*> session,
		const MTPWallPaper &data);
	[[nodiscard]] static std::optional<WallPaper> Create(
		not_null<Main::Session*> session,
		const MTPDwallPaper &data);
	[[nodiscard]] static std::optional<WallPaper> Create(
		const MTPDwallPaperNoFile &data);
#endif
	[[nodiscard]] static std::optional<WallPaper> Create(
		not_null<Main::Session*> session,
		const Tdb::TLbackground &paper);

	[[nodiscard]] QByteArray serialize() const;
	[[nodiscard]] static std::optional<WallPaper> FromSerialized(
		const QByteArray &serialized);
	[[nodiscard]] static std::optional<WallPaper> FromLegacySerialized(
		quint64 id,
		quint64 accessHash,
		quint32 flags,
		QString slug);
	[[nodiscard]] static std::optional<WallPaper> FromLegacyId(
		qint32 legacyId);
	[[nodiscard]] static std::optional<WallPaper> FromColorsSlug(
		const QString &slug);
	[[nodiscard]] static WallPaper FromEmojiId(const QString &emojiId);
	[[nodiscard]] static WallPaper ConstructDefault();

private:
	static constexpr auto kDefaultIntensity = 50;

	[[nodiscard]] QStringList collectShareParams() const;

	WallPaperId _id = WallPaperId();
	uint64 _accessHash = 0;
	UserId _ownerId = 0;
	WallPaperFlags _flags;
	QString _slug;
	QString _emojiId;

	std::vector<QColor> _backgroundColors;
	int _rotation = 0;
	int _intensity = kDefaultIntensity;
	bool _blurred = false;

	DocumentData *_document = nullptr;
	std::shared_ptr<Image> _thumbnail;

};

[[nodiscard]] WallPaper ThemeWallPaper();
[[nodiscard]] bool IsThemeWallPaper(const WallPaper &paper);
[[nodiscard]] WallPaper CustomWallPaper();
[[nodiscard]] bool IsCustomWallPaper(const WallPaper &paper);
[[nodiscard]] WallPaper Legacy1DefaultWallPaper();
[[nodiscard]] bool IsLegacy1DefaultWallPaper(const WallPaper &paper);
[[nodiscard]] bool IsLegacy2DefaultWallPaper(const WallPaper &paper);
[[nodiscard]] bool IsLegacy3DefaultWallPaper(const WallPaper &paper);
[[nodiscard]] bool IsLegacy4DefaultWallPaper(const WallPaper &paper);
[[nodiscard]] WallPaper DefaultWallPaper();
[[nodiscard]] bool IsDefaultWallPaper(const WallPaper &paper);
[[nodiscard]] bool IsCloudWallPaper(const WallPaper &paper);

[[nodiscard]] QImage GenerateDitheredGradient(const WallPaper &paper);

namespace details {

[[nodiscard]] WallPaper UninitializedWallPaper();
[[nodiscard]] bool IsUninitializedWallPaper(const WallPaper &paper);
[[nodiscard]] WallPaper TestingThemeWallPaper();
[[nodiscard]] bool IsTestingThemeWallPaper(const WallPaper &paper);
[[nodiscard]] WallPaper TestingDefaultWallPaper();
[[nodiscard]] bool IsTestingDefaultWallPaper(const WallPaper &paper);
[[nodiscard]] WallPaper TestingEditorWallPaper();
[[nodiscard]] bool IsTestingEditorWallPaper(const WallPaper &paper);

} // namespace details
} // namespace Data
