#pragma once

namespace DisabledReferenceIntegrityFix
{
	struct PluginInfo
	{
		static constexpr std::string_view NAME = "Disabled Reference Integrity Fix";
		static constexpr std::string_view AUTHOR = "Alaxouche";
		static constexpr std::string_view VERSION = "1.2.1";
		static constexpr std::string_view DESCRIPTION = "SKSE plugin to fix incorrectly disabled records [EXPERIMENTAL - no blacklist]";
		static constexpr uint32_t VERSION_MAJOR = 1;
		static constexpr uint32_t VERSION_MINOR = 2;
		static constexpr uint32_t VERSION_PATCH = 1;
	};
}
