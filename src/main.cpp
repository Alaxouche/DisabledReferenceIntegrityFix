#include "pch.h"
#include "config.h"
#include "logger.h"
#include "plugin.h"
#include "events.h"
#include "hooks.h"

using namespace DisabledReferenceIntegrityFix;

extern "C" [[maybe_unused]] __declspec(dllexport) bool SKSEAPI
SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	const RuntimeConfig cfg = LoadRuntimeConfig();
	ApplyConfig(cfg);

	SetupLog(cfg.enableLogging, cfg.logLevel);

	for (const auto& w : cfg.parseWarnings)
		logger::warn("{}", w);

	if (cfg.enableLogging) {
		logger::info("[Disabled Reference Integrity Fix] Initializing...");
		logger::info("[CONFIG] {}", cfg.iniFound ? "INI loaded" : "INI not found, using defaults");
		if (cfg.iniFound) {
			logger::info("[CONFIG] INI path: {}", cfg.iniPath.string());
		}
		logger::info("[CONFIG] ENABLE_LOGGING=true, LOG_LEVEL={}, VERBOSE_LOGGING={}",
			cfg.logLevel, cfg.verboseLogging);
		logger::info("[CONFIG] FIX_REFERENCES={}, FIX_NAVMESHES={}",
			cfg.fixReferences, cfg.fixNavmeshes);
		logger::info("[CONFIG] EARLY_FIX_ON_LOAD3D={}, AUTO_FIX_ON_CELL_LOAD(fallback)={}, INCLUDE_DELETED={}",
			cfg.earlyFixOnLoad3D, cfg.autoFixOnCellLoad, cfg.includeDeleted);
		logger::info("[CONFIG] PATCH_INTERIOR={}, PATCH_EXTERIOR={}",
			cfg.patchInterior, cfg.patchExterior);
	}

	SKSE::Init(a_skse);
	InstallRuntimeHooks();

	if (cfg.enableLogging) {
		logger::info("Plugin: {} v{}", PluginInfo::NAME, PluginInfo::VERSION);
		logger::info("Author: {}", PluginInfo::AUTHOR);
		logger::info("Description: {}", PluginInfo::DESCRIPTION);
		logger::info("");
		logger::info("Attempting to register messaging interface...");
	}

	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging) {
		logger::critical("ERROR - Failed to get messaging interface! PLUGIN WILL NOT WORK");
		return false;
	}

	if (cfg.enableLogging)
		logger::info("OK - Messaging interface obtained");

	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		logger::critical("ERROR - Failed to register message handler! PLUGIN WILL NOT WORK");
		return false;
	}

	if (cfg.enableLogging) {
		logger::info("OK - Message handler registered successfully");
		logger::info("[Disabled Reference Integrity Fix] Loaded");
	}

	return true;
}
