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
	const auto result = Execute(TLgetPhoneNumberInfoSync(
		tl_string(),
		tl_string(phone)
	));
	if (!result.has_value()) {
		return phone;
	} else {
		return QString("+%1 %2")
			.arg(result->data().vcountry_calling_code().v)
			.arg(result->data().vformatted_phone_number().v);
	}
}

} // namespace Tdb
