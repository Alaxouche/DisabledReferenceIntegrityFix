#pragma once

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

	// Running tally of everything the patcher has done this session.
	struct Stats
	{
		uint32_t total_refs_checked           = 0;
		uint32_t total_refs_fixed             = 0;
		uint32_t deleted_refs_fixed           = 0;
		uint32_t refs_below_min               = 0;
		uint32_t refs_initially_disabled_fixed = 0;
		uint32_t navmeshes_checked            = 0;
		uint32_t navmesh_vertices_fixed       = 0;
		uint32_t cells_processed              = 0;
	};

	// Per-worldspace numbers so the end-of-session summary has something useful to say.
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

	// What kind of fix (if any) we applied to a reference.
	enum class RefFixKind
	{
		None,
		Deleted,
		InitiallyDisabled,
		BelowMin,
	};

	extern Stats                                          g_stats;
	extern RE::ObjectRefHandle                            g_playerHandle;
	extern std::unordered_set<RE::FormID>                 g_processed_cells;
	extern std::unordered_map<RE::FormID, WorldspaceStats> g_worldspace_stats;
	extern bool                                           g_plugin_enabled;

	uint32_t FixCellReferences(RE::TESObjectCELL* a_cell);

	void FixAllLoadedCells();
}
