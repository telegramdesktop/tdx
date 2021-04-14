/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Tdb {

struct InstanceConfig {
	int32 apiId = 0;
	QString apiHash;
	QString systemLanguageCode;
	QString deviceModel;
	QString systemVersion;
	QString applicationVersion;
};

class Instance final {
public:
	explicit Instance(InstanceConfig &&config);
	~Instance();

	void testNetwork(Fn<void(bool)> done);

private:
	class Manager;
	class Impl;
	const std::unique_ptr<Impl> _impl;

	static std::weak_ptr<Instance::Manager> ManagerInstance;

};

} // namespace Tdb
