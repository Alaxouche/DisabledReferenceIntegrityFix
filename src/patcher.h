#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace DisabledReferenceIntegrityFix
{

	constexpr float Z_EPSILON = 0.01f;

	constexpr RE::FormID INTERIOR_WORLDSPACE_ID = 0xFFFFFFFF;

	constexpr std::size_t INITIAL_WORLDSPACE_CAPACITY = 16;

	constexpr std::size_t INITIAL_CELL_CAPACITY = 256;

	struct Stats
	{
		uint32_t total_refs_checked            = 0;
		uint32_t total_refs_fixed              = 0;
		uint32_t deleted_refs_fixed            = 0;
		uint32_t refs_below_min                = 0;
		uint32_t refs_initially_disabled_fixed = 0;
		uint32_t navmeshes_checked             = 0;
		uint32_t navmesh_vertices_fixed        = 0;
		uint32_t cells_processed               = 0;

		uint32_t fallback_event_cells_fixed            = 0;
		uint32_t fallback_event_refs_fixed             = 0;
		uint32_t fallback_event_navmesh_cells_fixed    = 0;
		uint32_t fallback_event_navmesh_vertices_fixed = 0;
	};

	struct HookStats
	{
		std::atomic<uint32_t> init_seen{0};
		std::atomic<uint32_t> init_fixed_pre_live{0};
		std::atomic<uint32_t> init_skipped_has3d{0};
		std::atomic<uint32_t> init_skipped_cell_attached{0};
		std::atomic<uint32_t> init_skipped_refs_fully_loaded{0};
		std::atomic<uint32_t> load3d_gated{0};
		std::atomic<uint32_t> init_cair_z_ok{0};
		std::atomic<uint32_t> init_excluded{0};

		void Reset() noexcept
		{
			init_seen.store(0, std::memory_order_relaxed);
			init_fixed_pre_live.store(0, std::memory_order_relaxed);
			init_skipped_has3d.store(0, std::memory_order_relaxed);
			init_skipped_cell_attached.store(0, std::memory_order_relaxed);
			init_skipped_refs_fully_loaded.store(0, std::memory_order_relaxed);
			load3d_gated.store(0, std::memory_order_relaxed);
			init_cair_z_ok.store(0, std::memory_order_relaxed);
			init_excluded.store(0, std::memory_order_relaxed);
		}
	};

	struct WorldspaceStats
	{
		std::string name;
		uint32_t    cells                  = 0;
		uint32_t    refs_fixed             = 0;
		uint32_t    deleted_fixed          = 0;
		uint32_t    init_disabled_fixed    = 0;
		uint32_t    below_min_fixed        = 0;
		uint32_t    navmesh_vertices_fixed = 0;
	};

	enum class RefFixKind
	{
		None,
		Deleted,
		InitiallyDisabled,
		BelowMin,
	};

	extern Stats                                           g_stats;
	extern HookStats                                       g_hook_stats;
	extern std::unordered_set<RE::FormID>                  g_processed_cells;
	extern std::unordered_map<RE::FormID, WorldspaceStats> g_worldspace_stats;
	extern bool                                            g_plugin_enabled;

	void LogRefFix(const char* tag, const RE::TESObjectREFR* ref, float oldZ, float newZ, const char* action);

	uint32_t FixCellReferences(RE::TESObjectCELL* a_cell);

	uint32_t FixCellNavmeshesOnly(RE::TESObjectCELL* a_cell);

	void FixAllLoadedCells();

	void LogHookInstrumentation(const char* a_reason);

	void MaybeLogHookInstrumentation(const char* a_reason, uint64_t a_intervalMs = 5000);
}
