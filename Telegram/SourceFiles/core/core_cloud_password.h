/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/bytes.h"

namespace Core {

#if 0 // mtp
constexpr auto kHandleSrpIdInvalidTimeout = 60 * crl::time(1000);

struct CloudPasswordAlgoModPow {
	static constexpr auto kIterations = 100000;

	bytes::vector salt1;
	bytes::vector salt2;
	int g = 0;
	bytes::vector p;
};

inline bool operator==(
		const CloudPasswordAlgoModPow &a,
		const CloudPasswordAlgoModPow &b) {
	return (a.salt1 == b.salt1)
		&& (a.salt2 == b.salt2)
		&& (a.g == b.g)
		&& (a.p == b.p);
}

using CloudPasswordAlgo = std::variant<v::null_t, CloudPasswordAlgoModPow>;

CloudPasswordAlgo ParseCloudPasswordAlgo(const MTPPasswordKdfAlgo &data);
CloudPasswordAlgo ValidateNewCloudPasswordAlgo(CloudPasswordAlgo &&parsed);
MTPPasswordKdfAlgo PrepareCloudPasswordAlgo(const CloudPasswordAlgo &data);

struct CloudPasswordCheckRequest {
	uint64 id = 0;
	bytes::vector B;
	CloudPasswordAlgo algo;

	explicit operator bool() const {
		return !v::is_null(algo);
	}
};

inline bool operator==(
		const CloudPasswordCheckRequest &a,
		const CloudPasswordCheckRequest &b) {
	return (a.id == b.id) && (a.B == b.B) && (a.algo == b.algo);
}

inline bool operator!=(
		const CloudPasswordCheckRequest &a,
		const CloudPasswordCheckRequest &b) {
	return !(a == b);
}

CloudPasswordCheckRequest ParseCloudPasswordCheckRequest(
	const MTPDaccount_password &data);
#endif

struct CloudPasswordResult {
#if 0 // mtp
	MTPInputCheckPasswordSRP result;
#endif
	QByteArray password;

#if 0 // mtp
	explicit operator bool() const;
#endif
};

#if 0 // mtp
struct CloudPasswordDigest {
	bytes::vector modpow;
};

bytes::vector ComputeCloudPasswordHash(
	const CloudPasswordAlgo &algo,
	bytes::const_span password);

CloudPasswordDigest ComputeCloudPasswordDigest(
	const CloudPasswordAlgo &algo,
	bytes::const_span password);

CloudPasswordResult ComputeCloudPasswordCheck(
	const CloudPasswordCheckRequest &request,
	bytes::const_span hash);

struct SecureSecretAlgoSHA512 {
	bytes::vector salt;
};

inline bool operator==(
		const SecureSecretAlgoSHA512 &a,
		const SecureSecretAlgoSHA512 &b) {
	return (a.salt == b.salt);
}

struct SecureSecretAlgoPBKDF2 {
	static constexpr auto kIterations = 100000;

	bytes::vector salt;
};

inline bool operator==(
		const SecureSecretAlgoPBKDF2 &a,
		const SecureSecretAlgoPBKDF2 &b) {
	return (a.salt == b.salt);
}

using SecureSecretAlgo = std::variant<
	v::null_t,
	SecureSecretAlgoSHA512,
	SecureSecretAlgoPBKDF2>;

SecureSecretAlgo ParseSecureSecretAlgo(
	const MTPSecurePasswordKdfAlgo &data);
SecureSecretAlgo ValidateNewSecureSecretAlgo(SecureSecretAlgo &&parsed);
MTPSecurePasswordKdfAlgo PrepareSecureSecretAlgo(
	const SecureSecretAlgo &data);

bytes::vector ComputeSecureSecretHash(
	const SecureSecretAlgo &algo,
	bytes::const_span password);
#endif

struct CloudPasswordState {
#if 0 // goodToRemove
	struct Mtp {
		CloudPasswordCheckRequest request;
		bool unknownAlgorithm = false;
		CloudPasswordAlgo newPassword;
		SecureSecretAlgo newSecureSecret;
	};
	Mtp mtp;
#endif
	bool hasPassword = false;
	bool hasRecovery = false;
	bool notEmptyPassport = false;
	bool outdatedClient = false;
	QString hint;
	QString unconfirmedPattern;
	TimeId pendingResetDate = 0;

	QString serverError;
};

#if 0 // mtp
CloudPasswordState ParseCloudPasswordState(
	const MTPDaccount_password &data);
#endif

} // namespace Core
