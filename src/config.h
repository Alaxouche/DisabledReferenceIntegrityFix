
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace DisabledReferenceIntegrityFix
{
	struct RuntimeConfig
	{
		bool        enableLogging    = true;
		bool        verboseLogging   = false;
		bool        fixReferences    = true;
		bool        fixNavmeshes     = false;
		bool        patchExterior    = true;
		bool        patchInterior    = true;
		bool        autoFixOnCellLoad = true;
		bool        includeDeleted   = false;
		bool        fixAllInitDisabled = false;
		uint32_t    maxRefsPerBatch  = 0;
		int         logLevel         = 3;
		bool        iniFound         = false;
		std::unordered_set<std::string> excludedMods;
		std::unordered_set<uint32_t>    excludedForms;
		std::filesystem::path iniPath;
		std::vector<std::string> parseWarnings;
	};

	RuntimeConfig LoadRuntimeConfig();

	void ApplyConfig(const RuntimeConfig& cfg);
}

namespace DisabledReferenceIntegrityFix::Config
{
	inline constexpr float Z_FLOOR = -30000.0f;

	extern bool ENABLE_LOGGING;

	extern bool VERBOSE_LOGGING;

	extern bool FIX_REFERENCES;

	extern bool FIX_NAVMESHES;

	extern bool PATCH_EXTERIOR;

	extern bool PATCH_INTERIOR;

	extern bool AUTO_FIX_ON_CELL_LOAD;

	extern bool INCLUDE_DELETED;

	extern bool FIX_ALL_INIT_DISABLED;

	extern uint32_t MAX_REFS_PER_BATCH;

	extern int LOG_LEVEL;

	extern std::unordered_set<std::string> EXCLUDED_MODS;

	extern std::unordered_set<uint32_t> EXCLUDED_FORMS;
}
