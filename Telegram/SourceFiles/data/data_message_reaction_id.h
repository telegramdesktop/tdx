/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/qt/qt_compare.h"

namespace Tdb {
class TLreactionType;
} // namespace Tdb

namespace Data {

struct ReactionId {
	std::variant<QString, DocumentId> data;

	[[nodiscard]] static QChar PaidTag() {
		return '*';
	}
	[[nodiscard]] static ReactionId Paid() {
		return { QString(PaidTag()) };
	}

	[[nodiscard]] bool empty() const {
		const auto emoji = std::get_if<QString>(&data);
		return emoji && emoji->isEmpty();
	}
	[[nodiscard]] bool paid() const {
		const auto emoji = std::get_if<QString>(&data);
		return emoji
			&& emoji->size() == 1
			&& emoji->at(0) == PaidTag();
	}
	[[nodiscard]] QString emoji() const {
		const auto emoji = std::get_if<QString>(&data);
		return (emoji && (emoji->size() != 1 || emoji->at(0) != PaidTag()))
			? *emoji
			: QString();
	}
	[[nodiscard]] DocumentId custom() const {
		const auto custom = std::get_if<DocumentId>(&data);
		return custom ? *custom : DocumentId();
	}

	explicit operator bool() const {
		return !empty();
	}

	friend inline auto operator<=>(
		const ReactionId &,
		const ReactionId &) = default;
	friend inline bool operator==(
		const ReactionId &a,
		const ReactionId &b) = default;
};

struct MessageReaction {
	ReactionId id;
	int count = 0;
	bool my = false;
};

[[nodiscard]] QString SearchTagToQuery(const ReactionId &tagId);
[[nodiscard]] ReactionId SearchTagFromQuery(const QString &query);
[[nodiscard]] std::vector<ReactionId> SearchTagsFromQuery(
	const QString &query);

[[nodiscard]] QString ReactionEntityData(const ReactionId &id);

#if 0 // mtp
[[nodiscard]] ReactionId ReactionFromMTP(const MTPReaction &reaction);
[[nodiscard]] MTPReaction ReactionToMTP(ReactionId id);
#endif

[[nodiscard]] ReactionId ReactionFromTL(const Tdb::TLreactionType &reaction);
[[nodiscard]] Tdb::TLreactionType ReactionToTL(ReactionId id);
[[nodiscard]] std::optional<Tdb::TLreactionType> ReactionToMaybeTL(
	ReactionId id);

} // namespace Data

Q_DECLARE_METATYPE(Data::ReactionId);
