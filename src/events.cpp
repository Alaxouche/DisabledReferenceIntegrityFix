#include "pch.h"
#include "events.h"
#include "patcher.h"
#include "config.h"

using namespace DisabledReferenceIntegrityFix::Config;

namespace DisabledReferenceIntegrityFix
{
	RE::BSEventNotifyControl CellFullyLoadedEventHandler::ProcessEvent(
		const RE::TESCellFullyLoadedEvent*           a_event,
		RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*)
	{
		if (!a_event) return RE::BSEventNotifyControl::kContinue;

		auto* cell = a_event->cell;
		if (!cell) return RE::BSEventNotifyControl::kContinue;

		g_processed_cells.erase(cell->GetFormID());

		if (AUTO_FIX_ON_CELL_LOAD) {
			const uint32_t fixed = FixCellReferences(cell);
			if (fixed > 0) {
				g_stats.fallback_event_cells_fixed++;
				g_stats.fallback_event_refs_fixed += fixed;
			}
		} else {
			const uint32_t navmeshFixed = FixCellNavmeshesOnly(cell);
			if (navmeshFixed > 0) {
				g_stats.fallback_event_cells_fixed++;
				g_stats.fallback_event_navmesh_cells_fixed++;
				g_stats.fallback_event_navmesh_vertices_fixed += navmeshFixed;
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	CellFullyLoadedEventHandler* CellFullyLoadedEventHandler::GetSingleton()
	{
		static CellFullyLoadedEventHandler singleton;
		return &singleton;
	}

	void MessageHandler(SKSE::MessagingInterface::Message* a_message)
	{
		if (!a_message) {
			logger::error("[Disabled Reference Integrity Fix] Null SKSE message");
			return;
		}

		switch (a_message->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			if (ENABLE_LOGGING)
				logger::info("[Disabled Reference Integrity Fix] Data loaded | refs:{} navmesh:{} early3D:{} cellLoad:{} int:{} ext:{} deleted:{} z:{:.0f}",
					FIX_REFERENCES?"on":"off", FIX_NAVMESHES?"on":"off",
					EARLY_FIX_ON_LOAD3D?"on":"off",
					AUTO_FIX_ON_CELL_LOAD?"on":"off",
					PATCH_INTERIOR?"on":"off", PATCH_EXTERIOR?"on":"off",
					INCLUDE_DELETED?"on":"off", Z_FLOOR);

			LogHookInstrumentation("data-loaded");

			if (AUTO_FIX_ON_CELL_LOAD || FIX_NAVMESHES) {
				auto* src = RE::ScriptEventSourceHolder::GetSingleton();
				if (src) {
					src->AddEventSink(CellFullyLoadedEventHandler::GetSingleton());
					if (ENABLE_LOGGING)
						logger::info("[Disabled Reference Integrity Fix] Cell-load sink registered (refs:{} navmesh:{})",
							AUTO_FIX_ON_CELL_LOAD ? "on" : "off",
							FIX_NAVMESHES ? "on" : "off");
				} else {
					logger::error("[Disabled Reference Integrity Fix] ScriptEventSourceHolder unavailable, event-driven fixes disabled");
				}
			}

			if (!EARLY_FIX_ON_LOAD3D) {
				if (ENABLE_LOGGING)
					logger::info("[Disabled Reference Integrity Fix] Hook-disabled fallback: scanning loaded cells at data-loaded");
				FixAllLoadedCells();
				LogHookInstrumentation("data-loaded-fallback-scan");
			}
			break;

		case SKSE::MessagingInterface::kPostLoadGame:
			if (ENABLE_LOGGING) logger::info("[Disabled Reference Integrity Fix] Save loaded, scanning...");
			FixAllLoadedCells();
			LogHookInstrumentation("post-load-game");
			break;

		case SKSE::MessagingInterface::kNewGame:
			if (ENABLE_LOGGING) logger::info("[Disabled Reference Integrity Fix] New game, scanning...");
			FixAllLoadedCells();
			LogHookInstrumentation("new-game");
			break;

		case SKSE::MessagingInterface::kPreLoadGame:
			g_stats = Stats{};
			g_processed_cells.clear();
			g_worldspace_stats.clear();
			if (ENABLE_LOGGING) logger::info("[Disabled Reference Integrity Fix] Pre-load: caches reset");
			break;

		default:
			if (VERBOSE_LOGGING && ENABLE_LOGGING)
				logger::trace("[Disabled Reference Integrity Fix] SKSE msg type={}", a_message->type);
			break;
		}
	}
}
