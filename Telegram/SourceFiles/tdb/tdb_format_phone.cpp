/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tdb/tdb_format_phone.h"

#include "tdb/tdb_account.h"

namespace Tdb {

QString FormatPhone(const QString &phone) {
	if (phone.isEmpty()) {
		return {};
	}
	const auto result = Execute(TLgetPhoneNumberInfoSync(
		tl_string(),
		tl_string(phone)
	));
	const auto fields = result ? &result->data() : nullptr;
	const auto code = fields ? fields->vcountry_calling_code().v : u""_q;
	const auto rest = fields ? fields->vformatted_phone_number().v : u""_q;
	return (code.isEmpty() && rest.isEmpty())
		? phone
		: u"+%1 %2"_q.arg(code).arg(rest);
}

// doLater Refuse to use "groups".
QVector<int> PhonePatternGroups(const QString &phone) {
	const auto filled = phone + QString().fill('X', 20 - phone.size());
	const auto formatted = FormatPhone(filled);
	const auto digits = QStringView(formatted).mid(1);
	QVector<int> groups;
	auto counter = 0;
	for (const auto &ch : digits) {
		if (ch == ' ') {
			groups.push_back(counter);
			counter = 0;
		} else {
			counter++;
		}
	}
	groups.push_back(counter);
	return groups;
}

} // namespace Tdb
