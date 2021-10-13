/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/qt/qt_string_view.h"

#include <QtCore/QRegularExpression>

namespace Passport {

[[nodiscard]] inline QDate ValidateDate(const QString &value) {
	const auto match = QRegularExpression(
		"^([0-9]{2})\\.([0-9]{2})\\.([0-9]{4})$").match(value);
	if (!match.hasMatch()) {
		return QDate();
	}
	auto result = QDate();
	const auto readInt = [](const QString &value) {
		auto view = QStringView(value);
		while (!view.isEmpty() && view.at(0) == '0') {
			view = base::StringViewMid(view, 1);
		}
		return view.toInt();
	};
	result.setDate(
		readInt(match.captured(3)),
		readInt(match.captured(2)),
		readInt(match.captured(1)));
	return result;
}

[[nodiscard]] inline QString FormatDate(int day, int month, int year) {
	const auto result = QString("%1.%2.%3"
		).arg(day, 2, 10, QChar('0')
		).arg(month, 2, 10, QChar('0')
		).arg(year, 4, 10, QChar('0'));
	return ValidateDate(result).isValid() ? result : QString();
}

} // namespace Passport
