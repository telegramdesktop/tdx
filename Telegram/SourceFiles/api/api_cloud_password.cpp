/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_cloud_password.h"

#include "apiwrap.h"
#include "base/random.h"
#include "core/core_cloud_password.h"
#include "passport/passport_encryption.h"

#include "base/unixtime.h"
#include "base/call_delayed.h"

namespace Api {
namespace {

using namespace Tdb;

int TLToCodeLength(const TLDpasswordState &data) {
	if (const auto email = data.vrecovery_email_address_code_info()) {
		return email->match([](
				const TLDemailAddressAuthenticationCodeInfo &data) {
			return data.vlength().v;
		});
	}
	return 0;
}

Core::CloudPasswordState TLToCloudPasswordState(
		const TLDpasswordState &data) {
	auto result = Core::CloudPasswordState();
	result.hasPassword = data.vhas_password().v;
	result.hasRecovery = data.vhas_recovery_email_address().v;
	result.notEmptyPassport = data.vhas_passport_data().v;
	result.hint = data.vpassword_hint().v;
	result.pendingResetDate = data.vpending_reset_date().v;

	if (const auto email = data.vrecovery_email_address_code_info()) {
		result.unconfirmedPattern = email->match([](
				const TLDemailAddressAuthenticationCodeInfo &data) {
			return data.vemail_address_pattern().v;
		});
	}

	return result;
}

[[nodiscard]] Core::CloudPasswordState ProcessMtpState(
		const MTPaccount_password &state) {
	return state.match([&](const MTPDaccount_password &data) {
		base::RandomAddSeed(bytes::make_span(data.vsecure_random().v));
		return Core::ParseCloudPasswordState(data);
	});
}

} // namespace

// #TODO Add ability to set recovery email separately.

CloudPassword::CloudPassword(not_null<ApiWrap*> api)
: _api(&api->instance()) {
}

CloudPassword::CloudPassword(Tdb::Sender &sender)
: _authorized(false)
, _api(&sender) {
}

void CloudPassword::apply(Core::CloudPasswordState state) {
	if (_state) {
		*_state = std::move(state);
	} else {
		_state = std::make_unique<Core::CloudPasswordState>(std::move(state));
	}
	_stateChanges.fire_copy(*_state);
}

void CloudPassword::reload() {
	if (_requestId) {
		return;
	}
#if 0 // goodToRemove
	_requestId = _api.request(MTPaccount_GetPassword(
	)).done([=](const MTPaccount_Password &result) {
		_requestId = 0;
		apply(ProcessMtpState(result));
	}).fail([=] {
		_requestId = 0;
	}).send();
#endif

	_api.request(TLgetPasswordState(
	)).done([=](const TLpasswordState &result) {
		apply(result);
	}).fail([=](const Error &error) {
		if (!_state) {
			_state = std::make_unique<Core::CloudPasswordState>();
		}
		_state->serverError = error.message;
		// See: td/telegram/PasswordManager.cpp.
		_state->outdatedClient = ((error.code == 400)
			&& (error.message == u"Please update client to continue"_q));
	}).send();
}

void CloudPassword::clearUnconfirmedPassword() {
	// There is no direct replacement for MTPaccount_CancelPasswordEmail,
	// but we can pass an empty info to Td::TLsetPassword
	// to clear an unconfirmed password.
	_requestId = _api.request(TLsetPassword(
		tl_string(),
		tl_string(),
		tl_string(),
		tl_bool(true),
		tl_string()
	)).done([=](const TLpasswordState &result) {
		_requestId = 0;
		apply(result);
	}).fail([=](const Error &error) {
		_requestId = 0;
		reload();
	}).send();

#if 0 // goodToRemove
	_requestId = _api.request(MTPaccount_CancelPasswordEmail(
	)).done([=] {
		_requestId = 0;
		reload();
	}).fail([=] {
		_requestId = 0;
		reload();
	}).send();
#endif
}

rpl::producer<Core::CloudPasswordState> CloudPassword::state() const {
	return _state
		? _stateChanges.events_starting_with_copy(*_state)
		: (_stateChanges.events() | rpl::type_erased());
}

auto CloudPassword::stateCurrent() const
-> std::optional<Core::CloudPasswordState> {
	return _state
		? base::make_optional(*_state)
		: std::nullopt;
}

#if 0 // goodToRemove
auto CloudPassword::resetPassword()
-> rpl::producer<CloudPassword::ResetRetryDate, QString> {
	return [=](auto consumer) {
		_api.request(MTPaccount_ResetPassword(
		)).done([=](const MTPaccount_ResetPasswordResult &result) {
			result.match([&](const MTPDaccount_resetPasswordOk &data) {
				reload();
			}, [&](const MTPDaccount_resetPasswordRequestedWait &data) {
				if (!_state) {
					reload();
					return;
				}
				const auto until = data.vuntil_date().v;
				if (_state->pendingResetDate != until) {
					_state->pendingResetDate = until;
					_stateChanges.fire_copy(*_state);
				}
			}, [&](const MTPDaccount_resetPasswordFailedWait &data) {
				consumer.put_next_copy(data.vretry_date().v);
			});
			consumer.put_done();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();

		return rpl::lifetime();
	};
}
#endif

auto CloudPassword::resetPassword()
-> rpl::producer<CloudPassword::ResetRetryDate, QString> {
	return [=](auto consumer) {
		_api.request(TLresetPassword(
		)).done([=](const TLresetPasswordResult &result) {
			result.match([&](const TLDresetPasswordResultOk &data) {
				reload();
			}, [&](const TLDresetPasswordResultPending &data) {
				if (!_state) {
					reload();
					return;
				}
				const auto until = data.vpending_reset_date().v;
				if (_state->pendingResetDate != until) {
					_state->pendingResetDate = until;
					_stateChanges.fire_copy(*_state);
				}
			}, [&](const TLDresetPasswordResultDeclined &data) {
				consumer.put_next_copy(data.vretry_date().v);
			});
			consumer.put_done();
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();

		return rpl::lifetime();
	};
}

auto CloudPassword::cancelResetPassword()
-> rpl::producer<rpl::no_value, QString> {
	return [=](auto consumer) {
#if 0 // goodToRemove
		_api.request(MTPaccount_DeclinePasswordReset(
		)).done([=] {
			reload();
			consumer.put_done();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
#endif
		_api.request(TLcancelPasswordReset(
		)).done([=] {
			reload();
			consumer.put_done();
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();

		return rpl::lifetime();
	};
}

void CloudPassword::apply(const TLpasswordState &state) {
	auto parsed = TLToCloudPasswordState(state.data());
	if (_state) {
		*_state = std::move(parsed);
	} else {
		_state = std::make_unique<Core::CloudPasswordState>(
			std::move(parsed));
	}
	_state->outdatedClient = false;
	_stateChanges.fire_copy(*_state);
}

rpl::producer<rpl::no_value, QString> CloudPassword::set(
		const QString &oldPassword,
		const QString &newPassword,
		const QString &hint,
		bool hasRecoveryEmail,
		const QString &recoveryEmail) {
	return [=](auto consumer) {
		_api.request(TLsetPassword(
			tl_string(oldPassword),
			tl_string(newPassword),
			tl_string(hint),
			tl_bool(hasRecoveryEmail),
			tl_string(recoveryEmail)
		)).done([=](const TLpasswordState &result) {
			// When a new password is set with a recovery email,
			// the server returns an error EMAIL_UNCONFIRMED_#
			// requiring email confirmation, but TDLib returns a success with
			// the given vrecovery_email_address_code_info optional parameter.
			// We simulate this error to keep an external behavior the same.
			const auto codeLength = TLToCodeLength(result.data());
			if (codeLength) {
				consumer.put_error(
					QString("EMAIL_UNCONFIRMED_%1").arg(codeLength));
			} else {
				consumer.put_done();
				apply(result);
			}
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();
#if 0 // doLater
		// }).handleFloodErrors().send();
#endif

		return rpl::lifetime();
	};
}

rpl::producer<rpl::no_value, QString> CloudPassword::confirmEmail(
		const QString &code) {
	return [=](auto consumer) {
		_api.request(TLcheckRecoveryEmailAddressCode(
			tl_string(code)
		)).done([=](const TLpasswordState &result) {
			consumer.put_done();
			apply(result);
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();
#if 0 // doLater
		// }).handleFloodErrors().send();
#endif

		return rpl::lifetime();
	};
}

rpl::producer<rpl::no_value, QString> CloudPassword::resendEmailCode() {
	return [=](auto consumer) {
		_api.request(TLresendRecoveryEmailAddressCode(
		)).done([=](const TLpasswordState &result) {
			consumer.put_done();
			apply(result);
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();

		return rpl::lifetime();
	};
}

rpl::producer<rpl::no_value, QString> CloudPassword::recoverPassword(
		const QString &code,
		const QString &newPassword,
		const QString &newHint) {
	return [=](auto consumer) {
		if (_authorized) {
			_api.request(TLrecoverPassword(
				tl_string(code),
				tl_string(newPassword),
				tl_string(newHint)
			)).done([=](const TLpasswordState &result) {
				consumer.put_done();
				apply(result);
			}).fail([=](const Error &error) {
				consumer.put_error_copy(error.message);
			}).send();
		} else {
			_api.request(TLrecoverAuthenticationPassword(
				tl_string(code),
				tl_string(newPassword),
				tl_string(newHint)
			)).done([=](const TLok &) {
				consumer.put_done();
			}).fail([=](const Error &error) {
				consumer.put_error_copy(error.message);
			}).send();
		}

		return rpl::lifetime();
	};
}

rpl::producer<QString, QString> CloudPassword::requestPasswordRecovery() {
	return [=](auto consumer) {
		if (!_authorized) {
			_api.request(TLrequestAuthenticationPasswordRecovery(
			)).done([=](const TLok &) {
				consumer.put_done();
			}).fail([=](const Error &error) {
				consumer.put_error_copy(error.message);
			}).send();
			return rpl::lifetime();
		}
		_api.request(TLrequestPasswordRecovery(
		)).done([=](const TLemailAddressAuthenticationCodeInfo &result) {
			result.match([&](
					const TLDemailAddressAuthenticationCodeInfo &data) {
				consumer.put_next_copy(data.vemail_address_pattern().v);
			});
			consumer.put_done();
		}).fail([=](const Error &error) {
			consumer.put_error_copy(error.message);
		}).send();

		return rpl::lifetime();
	};
}

auto CloudPassword::checkRecoveryEmailAddressCode(const QString &code)
-> rpl::producer<rpl::no_value, QString> {
	return [=](auto consumer) {
		if (_authorized) {
			_api.request(TLcheckRecoveryEmailAddressCode(
				tl_string(code)
			)).done([=](const TLpasswordState &) {
				consumer.put_done();
			}).fail([=](const Error &error) {
				consumer.put_error_copy(error.message);
			}).send();
#if 0 // doLater
			// }).handleFloodErrors().send();
#endif
		} else {
			_api.request(TLcheckAuthenticationPasswordRecoveryCode(
				tl_string(code)
			)).done([=](const TLok &) {
				consumer.put_done();
			}).fail([=](const Error &error) {
				consumer.put_error_copy(error.message);
			}).send();
		}

		return rpl::lifetime();
	};
}

rpl::producer<rpl::no_value, QString> CloudPassword::check(
		const QString &password) {
	return [=](auto consumer) {
		if (_authorized) {
			_api.request(Tdb::TLgetRecoveryEmailAddress(
				tl_string(password)
			)).done([=] {
				consumer.put_done();
			}).fail([=](const Error &error) {
				consumer.put_error_copy(error.message);
			}).send();
		} else {
			_api.request(TLcheckAuthenticationPassword(
				tl_string(password)
			)).done([=](const TLok &) {
				consumer.put_done();
			}).fail([=](const Error &error) {
				consumer.put_error_copy(error.message);
			}).send();
		}

		return rpl::lifetime();
	};
}

rpl::producer<CloudPassword::SetOk, QString> CloudPassword::set(
		const QString &oldPassword,
		const QString &newPassword,
		const QString &hint,
		bool hasRecoveryEmail,
		const QString &recoveryEmail) {

	const auto generatePasswordCheck = [=](
			const Core::CloudPasswordState &latestState) {
		if (oldPassword.isEmpty() || !latestState.hasPassword) {
			return Core::CloudPasswordResult{
				MTP_inputCheckPasswordEmpty()
			};
		}
		const auto hash = Core::ComputeCloudPasswordHash(
			latestState.mtp.request.algo,
			bytes::make_span(oldPassword.toUtf8()));
		return Core::ComputeCloudPasswordCheck(
			latestState.mtp.request,
			hash);
	};

	const auto finish = [=](auto consumer, int unconfirmedEmailLengthCode) {
		_api.request(MTPaccount_GetPassword(
		)).done([=](const MTPaccount_Password &result) {
			apply(ProcessMtpState(result));
			if (unconfirmedEmailLengthCode) {
				consumer.put_next(SetOk{ unconfirmedEmailLengthCode });
			} else {
				consumer.put_done();
			}
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).handleFloodErrors().send();
	};

	const auto sendMTPaccountUpdatePasswordSettings = [=](
			const Core::CloudPasswordState &latestState,
			const QByteArray &secureSecret,
			auto consumer) {
		const auto newPasswordBytes = newPassword.toUtf8();
		const auto newPasswordHash = Core::ComputeCloudPasswordDigest(
			latestState.mtp.newPassword,
			bytes::make_span(newPasswordBytes));
		if (!newPassword.isEmpty() && newPasswordHash.modpow.empty()) {
			consumer.put_error("INTERNAL_SERVER_ERROR");
			return;
		}
		using Flag = MTPDaccount_passwordInputSettings::Flag;
		const auto flags = Flag::f_new_algo
			| Flag::f_new_password_hash
			| Flag::f_hint
			| (secureSecret.isEmpty() ? Flag(0) : Flag::f_new_secure_settings)
			| ((!hasRecoveryEmail) ? Flag(0) : Flag::f_email);

		auto newSecureSecret = bytes::vector();
		auto newSecureSecretId = 0ULL;
		if (!secureSecret.isEmpty()) {
			newSecureSecretId = Passport::CountSecureSecretId(
				bytes::make_span(secureSecret));
			newSecureSecret = Passport::EncryptSecureSecret(
				bytes::make_span(secureSecret),
				Core::ComputeSecureSecretHash(
					latestState.mtp.newSecureSecret,
					bytes::make_span(newPasswordBytes)));
		}
		const auto settings = MTP_account_passwordInputSettings(
			MTP_flags(flags),
			Core::PrepareCloudPasswordAlgo(newPassword.isEmpty()
				? v::null
				: latestState.mtp.newPassword),
			newPassword.isEmpty()
				? MTP_bytes()
				: MTP_bytes(newPasswordHash.modpow),
			MTP_string(hint),
			MTP_string(recoveryEmail),
			MTP_secureSecretSettings(
				Core::PrepareSecureSecretAlgo(
					latestState.mtp.newSecureSecret),
				MTP_bytes(newSecureSecret),
				MTP_long(newSecureSecretId)));
		_api.request(MTPaccount_UpdatePasswordSettings(
			generatePasswordCheck(latestState).result,
			settings
		)).done([=] {
			finish(consumer, 0);
		}).fail([=](const MTP::Error &error) {
			const auto &type = error.type();
			const auto prefix = u"EMAIL_UNCONFIRMED_"_q;
			if (type.startsWith(prefix)) {
				const auto codeLength = base::StringViewMid(
					type,
					prefix.size()).toInt();

				finish(consumer, codeLength);
			} else {
				consumer.put_error_copy(type);
			}
		}).handleFloodErrors().send();
	};

	return [=](auto consumer) {
		_api.request(MTPaccount_GetPassword(
		)).done([=](const MTPaccount_Password &result) {
			const auto latestState = ProcessMtpState(result);

			if (latestState.hasPassword
					&& !oldPassword.isEmpty()
					&& !newPassword.isEmpty()) {

				_api.request(MTPaccount_GetPasswordSettings(
					generatePasswordCheck(latestState).result
				)).done([=](const MTPaccount_PasswordSettings &result) {
					using Settings = MTPDaccount_passwordSettings;
					const auto &data = result.match([&](
							const Settings &data) -> const Settings & {
						return data;
					});
					auto secureSecret = QByteArray();
					if (const auto wrapped = data.vsecure_settings()) {
						using Secure = MTPDsecureSecretSettings;
						const auto &settings = wrapped->match([](
								const Secure &data) -> const Secure & {
							return data;
						});
						const auto passwordUtf = oldPassword.toUtf8();
						const auto secret = Passport::DecryptSecureSecret(
							bytes::make_span(settings.vsecure_secret().v),
							Core::ComputeSecureSecretHash(
								Core::ParseSecureSecretAlgo(
									settings.vsecure_algo()),
								bytes::make_span(passwordUtf)));
						if (secret.empty()) {
							LOG(("API Error: "
								"Failed to decrypt secure secret."));
							consumer.put_error("SUGGEST_SECRET_RESET");
							return;
						} else if (Passport::CountSecureSecretId(secret)
								!= settings.vsecure_secret_id().v) {
							LOG(("API Error: Wrong secure secret id."));
							consumer.put_error("SUGGEST_SECRET_RESET");
							return;
						} else {
							secureSecret = QByteArray(
								reinterpret_cast<const char*>(secret.data()),
								secret.size());
						}
					}
					_api.request(MTPaccount_GetPassword(
					)).done([=](const MTPaccount_Password &result) {
						const auto latestState = ProcessMtpState(result);
						sendMTPaccountUpdatePasswordSettings(
							latestState,
							secureSecret,
							consumer);
					}).fail([=](const MTP::Error &error) {
						consumer.put_error_copy(error.type());
					}).send();
				}).fail([=](const MTP::Error &error) {
					consumer.put_error_copy(error.type());
				}).send();
			} else {
				sendMTPaccountUpdatePasswordSettings(
					latestState,
					QByteArray(),
					consumer);
			}
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
		return rpl::lifetime();
	};
}

rpl::producer<rpl::no_value, QString> CloudPassword::check(
		const QString &password) {
	return [=](auto consumer) {
		_api.request(MTPaccount_GetPassword(
		)).done([=](const MTPaccount_Password &result) {
			const auto latestState = ProcessMtpState(result);
			const auto input = [&] {
				if (password.isEmpty()) {
					return Core::CloudPasswordResult{
						MTP_inputCheckPasswordEmpty()
					};
				}
				const auto hash = Core::ComputeCloudPasswordHash(
					latestState.mtp.request.algo,
					bytes::make_span(password.toUtf8()));
				return Core::ComputeCloudPasswordCheck(
					latestState.mtp.request,
					hash);
			}();

			_api.request(MTPaccount_GetPasswordSettings(
				input.result
			)).done([=](const MTPaccount_PasswordSettings &result) {
				consumer.put_done();
			}).fail([=](const MTP::Error &error) {
				consumer.put_error_copy(error.type());
			}).send();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();

		return rpl::lifetime();
	};
}

rpl::producer<rpl::no_value, QString> CloudPassword::confirmEmail(
		const QString &code) {
	return [=](auto consumer) {
		_api.request(MTPaccount_ConfirmPasswordEmail(
			MTP_string(code)
		)).done([=] {
			_api.request(MTPaccount_GetPassword(
			)).done([=](const MTPaccount_Password &result) {
				apply(ProcessMtpState(result));
				consumer.put_done();
			}).fail([=](const MTP::Error &error) {
				consumer.put_error_copy(error.type());
			}).send();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).handleFloodErrors().send();

		return rpl::lifetime();
	};
}

rpl::producer<rpl::no_value, QString> CloudPassword::resendEmailCode() {
	return [=](auto consumer) {
		_api.request(MTPaccount_ResendPasswordEmail(
		)).done([=] {
			_api.request(MTPaccount_GetPassword(
			)).done([=](const MTPaccount_Password &result) {
				apply(ProcessMtpState(result));
				consumer.put_done();
			}).fail([=](const MTP::Error &error) {
				consumer.put_error_copy(error.type());
			}).send();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).handleFloodErrors().send();

		return rpl::lifetime();
	};
}

rpl::producer<CloudPassword::SetOk, QString> CloudPassword::setEmail(
		const QString &oldPassword,
		const QString &recoveryEmail) {
	const auto generatePasswordCheck = [=](
			const Core::CloudPasswordState &latestState) {
		if (oldPassword.isEmpty() || !latestState.hasPassword) {
			return Core::CloudPasswordResult{
				MTP_inputCheckPasswordEmpty()
			};
		}
		const auto hash = Core::ComputeCloudPasswordHash(
			latestState.mtp.request.algo,
			bytes::make_span(oldPassword.toUtf8()));
		return Core::ComputeCloudPasswordCheck(
			latestState.mtp.request,
			hash);
	};

	const auto finish = [=](auto consumer, int unconfirmedEmailLengthCode) {
		_api.request(MTPaccount_GetPassword(
		)).done([=](const MTPaccount_Password &result) {
			apply(ProcessMtpState(result));
			if (unconfirmedEmailLengthCode) {
				consumer.put_next(SetOk{ unconfirmedEmailLengthCode });
			} else {
				consumer.put_done();
			}
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).handleFloodErrors().send();
	};

	const auto sendMTPaccountUpdatePasswordSettings = [=](
			const Core::CloudPasswordState &latestState,
			auto consumer) {
		const auto settings = MTP_account_passwordInputSettings(
			MTP_flags(MTPDaccount_passwordInputSettings::Flag::f_email),
			MTP_passwordKdfAlgoUnknown(),
			MTP_bytes(),
			MTP_string(),
			MTP_string(recoveryEmail),
			MTPSecureSecretSettings());
		_api.request(MTPaccount_UpdatePasswordSettings(
			generatePasswordCheck(latestState).result,
			settings
		)).done([=] {
			finish(consumer, 0);
		}).fail([=](const MTP::Error &error) {
			const auto &type = error.type();
			const auto prefix = u"EMAIL_UNCONFIRMED_"_q;
			if (type.startsWith(prefix)) {
				const auto codeLength = base::StringViewMid(
					type,
					prefix.size()).toInt();

				finish(consumer, codeLength);
			} else {
				consumer.put_error_copy(type);
			}
		}).handleFloodErrors().send();
	};

	return [=](auto consumer) {
		_api.request(MTPaccount_GetPassword(
		)).done([=](const MTPaccount_Password &result) {
			const auto latestState = ProcessMtpState(result);
			sendMTPaccountUpdatePasswordSettings(latestState, consumer);
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
		return rpl::lifetime();
	};
}

rpl::producer<rpl::no_value, QString> CloudPassword::recoverPassword(
		const QString &code,
		const QString &newPassword,
		const QString &newHint) {

	const auto finish = [=](auto consumer) {
		_api.request(MTPaccount_GetPassword(
		)).done([=](const MTPaccount_Password &result) {
			apply(ProcessMtpState(result));
			consumer.put_done();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).handleFloodErrors().send();
	};

	const auto sendMTPaccountUpdatePasswordSettings = [=](
			const Core::CloudPasswordState &latestState,
			auto consumer) {
		const auto newPasswordBytes = newPassword.toUtf8();
		const auto newPasswordHash = Core::ComputeCloudPasswordDigest(
			latestState.mtp.newPassword,
			bytes::make_span(newPasswordBytes));
		if (!newPassword.isEmpty() && newPasswordHash.modpow.empty()) {
			consumer.put_error("INTERNAL_SERVER_ERROR");
			return;
		}
		using Flag = MTPDaccount_passwordInputSettings::Flag;
		const auto flags = Flag::f_new_algo
			| Flag::f_new_password_hash
			| Flag::f_hint;

		const auto settings = MTP_account_passwordInputSettings(
			MTP_flags(flags),
			Core::PrepareCloudPasswordAlgo(newPassword.isEmpty()
				? v::null
				: latestState.mtp.newPassword),
			newPassword.isEmpty()
				? MTP_bytes()
				: MTP_bytes(newPasswordHash.modpow),
			MTP_string(newHint),
			MTP_string(),
			MTPSecureSecretSettings());

		_api.request(MTPauth_RecoverPassword(
			MTP_flags(newPassword.isEmpty()
				? MTPauth_RecoverPassword::Flags(0)
				: MTPauth_RecoverPassword::Flag::f_new_settings),
			MTP_string(code),
			settings
		)).done([=](const MTPauth_Authorization &result) {
			finish(consumer);
		}).fail([=](const MTP::Error &error) {
			const auto &type = error.type();
			consumer.put_error_copy(type);
		}).handleFloodErrors().send();
	};

	return [=](auto consumer) {
		_api.request(MTPaccount_GetPassword(
		)).done([=](const MTPaccount_Password &result) {
			const auto latestState = ProcessMtpState(result);
			sendMTPaccountUpdatePasswordSettings(latestState, consumer);
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
		return rpl::lifetime();
	};
}

rpl::producer<QString, QString> CloudPassword::requestPasswordRecovery() {
	return [=](auto consumer) {
		_api.request(MTPauth_RequestPasswordRecovery(
		)).done([=](const MTPauth_PasswordRecovery &result) {
			result.match([&](const MTPDauth_passwordRecovery &data) {
				consumer.put_next(qs(data.vemail_pattern().v));
			});
			consumer.put_done();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).send();
		return rpl::lifetime();
	};
}

auto CloudPassword::checkRecoveryEmailAddressCode(const QString &code)
-> rpl::producer<rpl::no_value, QString> {
	return [=](auto consumer) {
		_api.request(MTPauth_CheckRecoveryPassword(
			MTP_string(code)
		)).done([=] {
			consumer.put_done();
		}).fail([=](const MTP::Error &error) {
			consumer.put_error_copy(error.type());
		}).handleFloodErrors().send();

		return rpl::lifetime();
	};
}

} // namespace Api
