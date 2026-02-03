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
		if (!a_event || !AUTO_FIX_ON_CELL_LOAD) return RE::BSEventNotifyControl::kContinue;

		auto* cell = a_event->cell;
		if (!cell) return RE::BSEventNotifyControl::kContinue;

		auto* player = RE::PlayerCharacter::GetSingleton();
		g_playerHandle = player
			? static_cast<RE::TESObjectREFR*>(player)->GetHandle()
			: RE::ObjectRefHandle{};

		FixCellReferences(cell);

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
				logger::info("[Disabled Reference Integrity Fix] Data loaded | refs:{} navmesh:{} cellLoad:{} int:{} ext:{} deleted:{} z:{:.0f}",
					FIX_REFERENCES?"on":"off", FIX_NAVMESHES?"on":"off",
					AUTO_FIX_ON_CELL_LOAD?"on":"off",
					PATCH_INTERIOR?"on":"off", PATCH_EXTERIOR?"on":"off",
					INCLUDE_DELETED?"on":"off", Z_FLOOR);

			if (AUTO_FIX_ON_CELL_LOAD) {
				auto* src = RE::ScriptEventSourceHolder::GetSingleton();
				if (src) {
					src->AddEventSink(CellFullyLoadedEventHandler::GetSingleton());
					if (ENABLE_LOGGING)
						logger::info("[Disabled Reference Integrity Fix] Cell-load sink registered");
				} else {
					logger::error("[Disabled Reference Integrity Fix] ScriptEventSourceHolder unavailable, cell-load auto-fix disabled");
				}
			}
			break;

		case SKSE::MessagingInterface::kPostLoadGame:
			if (ENABLE_LOGGING) logger::info("[Disabled Reference Integrity Fix] Save loaded, scanning...");
			FixAllLoadedCells();
			break;

		case SKSE::MessagingInterface::kNewGame:
			if (ENABLE_LOGGING) logger::info("[Disabled Reference Integrity Fix] New game, scanning...");
			FixAllLoadedCells();
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
