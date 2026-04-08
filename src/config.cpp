#include "pch.h"
#include "config.h"

#include <filesystem>
#include <fstream>
#include <cctype>
#include <string_view>
#include <Windows.h>

namespace DisabledReferenceIntegrityFix::Config
{
	bool     ENABLE_LOGGING     = true;
	bool     VERBOSE_LOGGING    = false;
	bool     FIX_REFERENCES     = true;
	bool     FIX_NAVMESHES      = false;
	bool     EARLY_FIX_ON_LOAD3D = true;
	bool     PATCH_EXTERIOR     = true;
	bool     PATCH_INTERIOR     = true;
	bool     AUTO_FIX_ON_CELL_LOAD = true;
	bool     INCLUDE_DELETED    = false;
	uint32_t MAX_REFS_PER_BATCH = 0;
	int      LOG_LEVEL          = 3;
	std::unordered_set<std::string> EXCLUDED_MODS;
	std::unordered_set<uint32_t>    EXCLUDED_FORMS;
}

namespace DisabledReferenceIntegrityFix
{

	namespace
	{
		constexpr uint32_t INI_LINE_RESERVE = 256;

		extern "C" IMAGE_DOS_HEADER __ImageBase;

		std::filesystem::path GetThisModulePath()
		{
			wchar_t buffer[MAX_PATH] = {};
			const auto len = ::GetModuleFileNameW(
				reinterpret_cast<HMODULE>(&__ImageBase),
				buffer,
				static_cast<DWORD>(std::size(buffer)));
			if (len == 0 || len >= std::size(buffer)) {
				return {};
			}
			return std::filesystem::path(buffer, buffer + len);
		}

		std::string_view TrimView(std::string_view s)
		{
			constexpr std::string_view kWhitespace = " \t\r\n";
			const auto start = s.find_first_not_of(kWhitespace);
			if (start == std::string_view::npos) return {};
			const auto end = s.find_last_not_of(kWhitespace);
			return s.substr(start, end - start + 1);
		}

		std::string ToLower(std::string_view s)
		{
			std::string result(s);
			for (auto& ch : result)
				ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
			return result;
		}

		bool ParseBool(std::string_view v)
		{
			if (v.empty()) return false;
			const char c0 = static_cast<char>(std::tolower(static_cast<unsigned char>(v[0])));
			if (c0 == '1' || c0 == 't' || c0 == 'y') return true;
			if (c0 == 'o') {
				if (v.size() >= 2) {
					const char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(v[1])));
					return c1 == 'n';
				}
				return false;
			}
			return false;
		}
	}

	RuntimeConfig LoadRuntimeConfig()
	{
		RuntimeConfig cfg{};

		std::vector<std::filesystem::path> candidates;
		candidates.reserve(4);
		if (auto modulePath = GetThisModulePath(); !modulePath.empty()) {
			candidates.push_back(modulePath.parent_path() / "DisabledReferenceIntegrityFix.ini");
			auto sameNameIni = modulePath;
			sameNameIni.replace_extension(".ini");
			candidates.push_back(sameNameIni);
		}
		candidates.emplace_back("Data/SKSE/Plugins/DisabledReferenceIntegrityFix.ini");
		candidates.emplace_back("SKSE/Plugins/DisabledReferenceIntegrityFix.ini");

		for (const auto& path : candidates) {
			std::error_code ec;
			if (std::filesystem::exists(path, ec)) {
				cfg.iniFound = true;
				cfg.iniPath  = path;
				break;
			}
		}

		if (!cfg.iniFound) {
			return cfg;
		}

		std::ifstream in(cfg.iniPath);
		if (!in.is_open()) {
			cfg.iniFound = false;
			cfg.iniPath.clear();
			return cfg;
		}

		std::string line;
		line.reserve(INI_LINE_RESERVE);

		// Splits on commas only — dash is NOT a separator because mod filenames can contain hyphens.
		auto ForEachCommaSeparated = [](std::string_view input, auto&& onToken) {
			size_t start = 0;
			for (size_t i = 0; i <= input.size(); ++i) {
				const bool atEnd = i == input.size();
				if (atEnd || input[i] == ',') {
					const std::string_view token = TrimView(input.substr(start, i - start));
					if (!token.empty()) onToken(token);
					start = i + 1;
				}
			}
		};

		// Splits on commas and dashes — safe for hex FormIDs which never contain hyphens.
		auto ForEachFormToken = [](std::string_view input, auto&& onToken) {
			size_t start = 0;
			for (size_t i = 0; i <= input.size(); ++i) {
				const bool atEnd = i == input.size();
				const char ch = atEnd ? '\0' : input[i];
				if (atEnd || ch == ',' || ch == '-') {
					const std::string_view token = TrimView(input.substr(start, i - start));
					if (!token.empty()) onToken(token);
					start = i + 1;
				}
			}
		};

		while (std::getline(in, line)) {
			if (auto p = line.find(';'); p != std::string::npos) line.resize(p);
			if (auto p = line.find('#'); p != std::string::npos) line.resize(p);

			const std::string_view lineView = TrimView(line);
			if (lineView.empty() || lineView.front() == '[') continue;

			const auto eq = lineView.find('=');
			if (eq == std::string_view::npos) continue;

			const std::string key = ToLower(TrimView(lineView.substr(0, eq)));
			const std::string_view val = TrimView(lineView.substr(eq + 1));

			try {
				if      (key == "enable_logging")       { cfg.enableLogging     = ParseBool(val); }
				else if (key == "verbose_logging")       { cfg.verboseLogging    = ParseBool(val); }
				else if (key == "fix_references")        { cfg.fixReferences     = ParseBool(val); }
				else if (key == "fix_navmeshes")         { cfg.fixNavmeshes      = ParseBool(val); }
				else if (key == "early_fix_on_load3d")   { cfg.earlyFixOnLoad3D  = ParseBool(val); }
				else if (key == "auto_fix_on_cell_load") { cfg.autoFixOnCellLoad = ParseBool(val); }
	
				else if (key == "patch_interior")        { cfg.patchInterior     = ParseBool(val); }
				else if (key == "patch_exterior")        { cfg.patchExterior     = ParseBool(val); }
				else if (key == "include_deleted_refs")  { cfg.includeDeleted    = ParseBool(val); }
				else if (key == "max_refs_per_batch")    { cfg.maxRefsPerBatch   = static_cast<uint32_t>(std::stoul(std::string(val))); }
				else if (key == "log_level") {
					cfg.logLevel = std::stoi(std::string(val));
					cfg.logLevel = std::clamp(cfg.logLevel, 1, 5);
				}
				else if (key == "excludemod") {
					ForEachCommaSeparated(val, [&](std::string_view token) {
						cfg.excludedMods.insert(ToLower(token));
					});
				}
				else if (key == "excludeform") {
					ForEachFormToken(val, [&](std::string_view token) {
						try {
							const uint32_t id = static_cast<uint32_t>(std::stoul(std::string(token), nullptr, 0));
							cfg.excludedForms.insert(id);
						} catch (const std::exception& e) {
							cfg.parseWarnings.push_back(
								std::format("[CONFIG] Malformed excludeform token \"{}\" ({})", token, e.what()));
						}
					});
				}
			} catch (const std::exception& e) {
				cfg.parseWarnings.push_back(
					std::format("[CONFIG] Malformed INI value for key \"{}\" ({})", key, e.what()));
			}
		}

		cfg.logLevel = std::clamp(cfg.logLevel, 1, 5);

		return cfg;
	}

	void ApplyConfig(const RuntimeConfig& cfg)
	{
		using namespace Config;
		ENABLE_LOGGING      = cfg.enableLogging;
		VERBOSE_LOGGING     = cfg.verboseLogging;
		FIX_REFERENCES      = cfg.fixReferences;
		FIX_NAVMESHES       = cfg.fixNavmeshes;
		EARLY_FIX_ON_LOAD3D = cfg.earlyFixOnLoad3D;
		PATCH_EXTERIOR      = cfg.patchExterior;
		PATCH_INTERIOR      = cfg.patchInterior;
		AUTO_FIX_ON_CELL_LOAD = cfg.autoFixOnCellLoad;
		INCLUDE_DELETED     = cfg.includeDeleted;
		MAX_REFS_PER_BATCH  = cfg.maxRefsPerBatch;
		LOG_LEVEL           = cfg.logLevel;
		EXCLUDED_MODS       = cfg.excludedMods;
		EXCLUDED_FORMS      = cfg.excludedForms;
	}
}
