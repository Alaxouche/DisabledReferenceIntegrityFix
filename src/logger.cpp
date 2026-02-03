#include "pch.h"
#include "logger.h"

namespace
{
	spdlog::level::level_enum ToLevel(int logLevel)
	{
		switch (logLevel) {
		case 1:  return spdlog::level::err;
		case 2:  return spdlog::level::warn;
		case 3:  return spdlog::level::info;
		case 4:  return spdlog::level::debug;
		case 5:  return spdlog::level::trace;
		default: return spdlog::level::info;
		}
	}
}

void SetupLog(bool enableLogging, int logLevel)
{
	std::vector<spdlog::sink_ptr> sinks;
	sinks.reserve(2); 
	spdlog::level::level_enum lvl = spdlog::level::off;

	if (enableLogging) {
		auto logDir = logger::log_directory();
		if (logDir) {
			*logDir /= "DisabledReferenceIntegrityFix.log";
			sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(logDir->string(), true));
		} else {
			sinks.push_back(std::make_shared<spdlog::sinks::null_sink_mt>());
		}

#ifndef NDEBUG
		sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif

		lvl = ToLevel(logLevel);
	} else {
		sinks.push_back(std::make_shared<spdlog::sinks::null_sink_mt>());
	}

	auto combinedLogger = std::make_shared<spdlog::logger>("global log", sinks.begin(), sinks.end());
	combinedLogger->set_level(lvl);
	combinedLogger->flush_on(spdlog::level::info);
	spdlog::set_default_logger(combinedLogger);
	spdlog::set_level(lvl);
	spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");
}
