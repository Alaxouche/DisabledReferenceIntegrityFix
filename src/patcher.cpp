#include "pch.h"
#include "patcher.h"
#include "config.h"
#include "utils.h"

using namespace DisabledReferenceIntegrityFix::Config;

namespace DisabledReferenceIntegrityFix
{

	Stats                                           g_stats{};
	RE::ObjectRefHandle                             g_playerHandle{};
	std::unordered_set<RE::FormID>                  g_processed_cells{};
	std::unordered_map<RE::FormID, WorldspaceStats> g_worldspace_stats{};
	bool                                            g_plugin_enabled = true;

	namespace
	{
		std::atomic<uint64_t> g_last_hook_log_ms{ 0 };

		bool IsExcludedByConfig(const RE::TESObjectREFR* ref)
		{
			if (!ref) return true;
			if (EXCLUDED_FORMS.contains(ref->GetFormID())) return true;
			if (IsFormFromExcludedMod(ref)) return true;
			if (const auto* base = ref->GetBaseObject()) {
				if (EXCLUDED_FORMS.contains(base->GetFormID())) return true;
				if (IsFormFromExcludedMod(base)) return true;
			}
			return false;
		}

		bool IsZBelowMin(float z)
		{
			return z < Z_FLOOR;
		}

		const char* FormTypeTag(const RE::TESForm* form)
		{
			if (!form) return "?";
			switch (form->GetFormType()) {
			case RE::FormType::Static:        return "STAT";
			case RE::FormType::Tree:          return "TREE";
			case RE::FormType::Activator:     return "ACTI";
			case RE::FormType::MovableStatic: return "MSTT";
			case RE::FormType::Flora:         return "FLOR";
			case RE::FormType::Furniture:     return "FURN";
			case RE::FormType::Light:         return "LIGH";
			case RE::FormType::Door:          return "DOOR";
			case RE::FormType::Container:     return "CONT";
			default:                          return "????";
			}
		}

		RE::NiPoint3 GetRefLocation(const RE::TESObjectREFR* ref)
		{
			return ref ? ref->data.location : RE::NiPoint3{};
		}

		void SetRefLocation(RE::TESObjectREFR* ref, const RE::NiPoint3& pos)
		{
			if (!ref) return;
			ref->data.location = pos;
			if (ref->Get3D()) {
				ref->Update3DPosition(true);
			}
		}

		void AttachPlayerEnableParentOpposite(RE::TESObjectREFR* ref)
		{
			if (!ref || !g_playerHandle) return;

			if (ref->extraList.HasType<RE::ExtraEnableStateParent>()) {
				if (VERBOSE_LOGGING && ENABLE_LOGGING)
					logger::trace("[enable-parent] 0x{:08X} already has parent, skipping", ref->GetFormID());
				return;
			}

			auto* parentExtra = RE::BSExtraData::Create<RE::ExtraEnableStateParent>();
			if (!parentExtra) {
				if (ENABLE_LOGGING) logger::warn("[enable-parent] 0x{:08X} alloc failed", ref->GetFormID());
				return;
			}

			parentExtra->flags  = 1;
			parentExtra->parent = g_playerHandle;

			if (ref->extraList.Add(parentExtra) == nullptr) {
				if (ENABLE_LOGGING) logger::warn("[enable-parent] 0x{:08X} Add() failed", ref->GetFormID());
				RE::free(parentExtra);
			}
		}

		RefFixKind FixDeletedReference(RE::TESObjectREFR* ref)
		{
			if (!ref || !ref->IsDeleted()) return RefFixKind::None;
			if (IsExcludedByConfig(ref)) return RefFixKind::None;

			auto* base = ref->GetBaseObject();
			if (!base) {
				if (ENABLE_LOGGING)
					logger::warn("[DELETED] 0x{:08X}: no base object, skipped", ref->GetFormID());
				return RefFixKind::None;
			}
			if (IsMarkerBase(base)) return RefFixKind::None;

			ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInitiallyDisabled;
			ref->formFlags &= ~RE::TESObjectREFR::RecordFlags::kDeleted;

			auto pos = GetRefLocation(ref);
			if (std::fabs(pos.z - Z_FLOOR) > Z_EPSILON) {
				RE::NiPoint3 newPos = pos;
				newPos.z = Z_FLOOR;
				SetRefLocation(ref, newPos);
			}

			if (!ref->IsDisabled()) ref->Disable();
			AttachPlayerEnableParentOpposite(ref);

			LogRefFix("DELETED", ref, pos.z, Z_FLOOR, "Recover+InitDisabled+EnableParent");

			g_stats.deleted_refs_fixed++;
			g_stats.total_refs_fixed++;
			return RefFixKind::Deleted;
		}

		RefFixKind FixReferenceZ(RE::TESObjectREFR* ref)
		{
			if (!ref || !FIX_REFERENCES) return RefFixKind::None;
			if (IsExcludedByConfig(ref)) return RefFixKind::None;

			const auto currentPos = GetRefLocation(ref);
			const float z = currentPos.z;
			auto* base = ref->GetBaseObject();

			const bool canApplyInitDisabledRule = ref->IsInitiallyDisabled() &&
				!ref->IsPersistent() &&
				!ref->HasQuestObject() &&
				!ref->extraList.HasType<RE::ExtraEnableStateParent>();

			if (canApplyInitDisabledRule) {
				if (base && !IsMarkerBase(base) && std::fabs(z - Z_FLOOR) > Z_EPSILON) {
					RE::NiPoint3 newPos = currentPos;
					newPos.z = Z_FLOOR;
					SetRefLocation(ref, newPos);
					LogRefFix("INIT-DISBL", ref, z, Z_FLOOR, "R2(user-rule)");
					g_stats.refs_initially_disabled_fixed++;
					g_stats.total_refs_fixed++;
					return RefFixKind::InitiallyDisabled;
				}
				return RefFixKind::None;
			}

			if (z <= Z_FLOOR && !ref->IsInitiallyDisabled()) {
				if (!base || IsMarkerBase(base) || ref->IsDeleted()) return RefFixKind::None;
				ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInitiallyDisabled;
				if (!ref->IsDisabled()) ref->Disable();
				AttachPlayerEnableParentOpposite(ref);
				LogRefFix("BELOW-30K", ref, z, z, "InitDisabled+Disable+EnableParent");
				g_stats.refs_below_min++;
				g_stats.total_refs_fixed++;
				return RefFixKind::BelowMin;
			}

			return RefFixKind::None;
		}

		uint32_t FixNavmeshVertices(RE::TESObjectCELL* cell)
		{
			if (!cell || !FIX_NAVMESHES) return 0;

			uint32_t fixed = 0;

			try {
				auto* navMeshes = cell->GetRuntimeData().navMeshes;
				if (!navMeshes || navMeshes->navMeshes.empty()) return 0;

				auto& arr = navMeshes->navMeshes;
				for (uint32_t i = 0; i < arr.size(); ++i) {
					auto* nm = arr[i].get();
					if (!nm) continue;
					g_stats.navmeshes_checked++;
					auto& verts = nm->vertices;
					for (uint32_t j = 0; j < verts.size(); ++j) {
						auto& v = verts[j];
						if (IsZBelowMin(v.location.z)) {
							if (VERBOSE_LOGGING && ENABLE_LOGGING)
								logger::trace("[navmesh] vtx {} Z:{:.0f}->{:.0f}", j, v.location.z, Z_FLOOR);
							v.location.z = Z_FLOOR;
							++fixed;
						}
					}
				}

				if (fixed > 0 && ENABLE_LOGGING)
					logger::info("[NAVMESH] Cell 0x{:08X}: {} vertices Z-clamped to {:.0f}",
						cell->GetFormID(), fixed, Z_FLOOR);

			} catch (const std::exception& e) {
				logger::error("[navmesh] {}", e.what());
			} catch (...) {
				logger::error("[navmesh] unknown exception");
			}

			g_stats.navmesh_vertices_fixed += fixed;
			return fixed;
		}

	}

	void LogRefFix(const char* tag, const RE::TESObjectREFR* ref, float oldZ, float newZ, const char* action)
	{
		if (!ENABLE_LOGGING || !ref) return;

		auto* base = ref->GetBaseObject();

		auto isReal = [](const char* s) { return s && s[0] && s[0] != '<'; };

		const char* dispName = base ? base->GetName() : nullptr;
		const char* baseEdid = base ? base->GetFormEditorID() : nullptr;
		const char* label    = isReal(dispName) ? dispName
			             : isReal(baseEdid) ? baseEdid
			             : FormTypeTag(base);

		auto* cell = ref->GetParentCell();
		const auto cellID   = cell ? cell->GetFormID() : 0;
		const auto cellType = (cell && cell->IsInteriorCell()) ? "int" : "ext";
		int32_t cellX = 0, cellY = 0;
		if (cell) {
			if (auto* ext = cell->GetCoordinates()) {
				cellX = ext->cellX;
				cellY = ext->cellY;
			}
		}

		if (std::fabs(oldZ - newZ) > Z_EPSILON)
			logger::info("[{}] ref=0x{:08X} base=0x{:08X} \"{}\" Z:{:.0f}->{:.0f} action={} cell=0x{:08X} cellType={} cellXY=({}, {}) srcRef={} srcBase={}",
				tag, ref->GetFormID(), base ? base->GetFormID() : 0, label,
				oldZ, newZ, action, cellID, cellType, cellX, cellY,
				FormSourceFile(ref), FormSourceFile(base));
		else
			logger::info("[{}] ref=0x{:08X} base=0x{:08X} \"{}\" Z:{:.0f} action={} cell=0x{:08X} cellType={} cellXY=({}, {}) srcRef={} srcBase={}",
				tag, ref->GetFormID(), base ? base->GetFormID() : 0, label,
				oldZ, action, cellID, cellType, cellX, cellY,
				FormSourceFile(ref), FormSourceFile(base));
	}

	uint32_t FixCellReferences(RE::TESObjectCELL* cell)
	{
		if (!cell || !g_plugin_enabled) return 0;

		const auto cellFormID = cell->GetFormID();
		if (g_processed_cells.contains(cellFormID)) return 0;

		const bool isInterior = cell->IsInteriorCell();

		RE::FormID  worldspaceID   = isInterior ? INTERIOR_WORLDSPACE_ID : 0;
		std::string worldspaceName = "Interior";
		if (!isInterior) {
			if (auto* ws = cell->GetRuntimeData().worldSpace) {
				worldspaceID = ws->GetFormID();
				if (auto* wsName = ws->GetName(); wsName && wsName[0])
					worldspaceName = wsName;
				else
					worldspaceName = std::format("Worldspace_0x{:08X}", worldspaceID);
			}
		}

		if (!g_worldspace_stats.contains(worldspaceID))
			g_worldspace_stats[worldspaceID].name = worldspaceName;
		auto& wsStats = g_worldspace_stats[worldspaceID];

		if (isInterior  && !PATCH_INTERIOR) return 0;
		if (!isInterior && !PATCH_EXTERIOR)  return 0;

		wsStats.cells++;

		if (VERBOSE_LOGGING && ENABLE_LOGGING) {
			const char* cellName = cell->GetName();
			logger::debug("[cell] {} (0x{:08X}) {}",
				cellName ? cellName : "?", cellFormID, isInterior ? "Int" : "Ext");
		}

		uint32_t fixedCount = 0;

		cell->ForEachReference([&](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
			if (!ref) return RE::BSContainer::ForEachResult::kContinue;

			g_stats.total_refs_checked++;
			bool processed = false;

			if (INCLUDE_DELETED) {
				if (FixDeletedReference(ref) == RefFixKind::Deleted) {
					fixedCount++;
					wsStats.refs_fixed++;
					wsStats.deleted_fixed++;
					processed = true;
				}
			}

			if (!processed) {
				switch (FixReferenceZ(ref)) {
				case RefFixKind::InitiallyDisabled:
					fixedCount++;
					wsStats.refs_fixed++;
					wsStats.init_disabled_fixed++;
					break;
				case RefFixKind::BelowMin:
					fixedCount++;
					wsStats.refs_fixed++;
					wsStats.below_min_fixed++;
					break;
				default:
					break;
				}
			}

			if (MAX_REFS_PER_BATCH > 0 && fixedCount >= MAX_REFS_PER_BATCH) {
				if (ENABLE_LOGGING)
					logger::warn("[cell] Batch limit ({}) reached", MAX_REFS_PER_BATCH);
				return RE::BSContainer::ForEachResult::kStop;
			}

			return RE::BSContainer::ForEachResult::kContinue;
		});

		const uint32_t navmeshFixes = FixNavmeshVertices(cell);
		wsStats.navmesh_vertices_fixed += navmeshFixes;

		if ((fixedCount > 0 || navmeshFixes > 0) && ENABLE_LOGGING) {
			const char* cellName = cell->GetName();
			logger::info("[cell] {} (0x{:08X}) {}: {} fix(es)",
				cellName ? cellName : "?", cellFormID,
				isInterior ? "Int" : "Ext",
				fixedCount + navmeshFixes);
		}

		g_processed_cells.insert(cellFormID);
		g_stats.cells_processed++;

		return fixedCount + navmeshFixes;
	}

	uint32_t FixCellNavmeshesOnly(RE::TESObjectCELL* cell)
	{
		if (!cell || !g_plugin_enabled || !FIX_NAVMESHES) return 0;

		const bool isInterior = cell->IsInteriorCell();
		if (isInterior && !PATCH_INTERIOR) return 0;
		if (!isInterior && !PATCH_EXTERIOR) return 0;

		const uint32_t navmeshFixes = FixNavmeshVertices(cell);
		if (navmeshFixes > 0 && ENABLE_LOGGING) {
			const char* cellName = cell->GetName();
			logger::info("[cell-navmesh] {} (0x{:08X}) {}: {} navmesh vert(s) fixed",
				cellName ? cellName : "?",
				cell->GetFormID(),
				isInterior ? "Int" : "Ext",
				navmeshFixes);
		}

		return navmeshFixes;
	}

	void FixAllLoadedCells()
	{
		const auto hook_init_seen                      = g_stats.hook_init_seen;
		const auto hook_init_fixed_pre_live            = g_stats.hook_init_fixed_pre_live;
		const auto hook_init_skipped_has3d             = g_stats.hook_init_skipped_has3d;
		const auto hook_init_skipped_cell_attached     = g_stats.hook_init_skipped_cell_attached;
		const auto hook_init_skipped_refs_fully_loaded = g_stats.hook_init_skipped_refs_fully_loaded;
		const auto hook_load3d_gated                   = g_stats.hook_load3d_gated;
		const auto fallback_event_cells_fixed          = g_stats.fallback_event_cells_fixed;
		const auto fallback_event_refs_fixed           = g_stats.fallback_event_refs_fixed;
		const auto fallback_event_navmesh_cells_fixed  = g_stats.fallback_event_navmesh_cells_fixed;
		const auto fallback_event_navmesh_vertices_fixed = g_stats.fallback_event_navmesh_vertices_fixed;
		const auto hook_init_cair_z_ok                 = g_stats.hook_init_cair_z_ok;
		const auto hook_init_excluded                  = g_stats.hook_init_excluded;

		g_processed_cells.clear();
		g_processed_cells.reserve(INITIAL_CELL_CAPACITY);
		g_worldspace_stats.clear();
		g_worldspace_stats.reserve(INITIAL_WORLDSPACE_CAPACITY);
		g_stats = Stats{};
		g_stats.hook_init_seen                      = hook_init_seen;
		g_stats.hook_init_fixed_pre_live            = hook_init_fixed_pre_live;
		g_stats.hook_init_skipped_has3d             = hook_init_skipped_has3d;
		g_stats.hook_init_skipped_cell_attached     = hook_init_skipped_cell_attached;
		g_stats.hook_init_skipped_refs_fully_loaded = hook_init_skipped_refs_fully_loaded;
		g_stats.hook_load3d_gated                   = hook_load3d_gated;
		g_stats.fallback_event_cells_fixed          = fallback_event_cells_fixed;
		g_stats.fallback_event_refs_fixed           = fallback_event_refs_fixed;
		g_stats.fallback_event_navmesh_cells_fixed  = fallback_event_navmesh_cells_fixed;
		g_stats.fallback_event_navmesh_vertices_fixed = fallback_event_navmesh_vertices_fixed;
		g_stats.hook_init_cair_z_ok                 = hook_init_cair_z_ok;
		g_stats.hook_init_excluded                  = hook_init_excluded;

		auto* player = RE::PlayerCharacter::GetSingleton();
		g_playerHandle = player
			? static_cast<RE::TESObjectREFR*>(player)->GetHandle()
			: RE::ObjectRefHandle{};

		auto* tes = RE::TES::GetSingleton();
		if (!tes) {
			logger::error("[Disabled Reference Integrity Fix] TES singleton unavailable, scan aborted");
			return;
		}

		if (ENABLE_LOGGING)
			logger::info("[Disabled Reference Integrity Fix] Scanning all loaded cells...");

		tes->ForEachCell([&](RE::TESObjectCELL* cell) {
			if (cell) FixCellReferences(cell);
		});

		if (ENABLE_LOGGING) {
			const uint32_t total = g_stats.total_refs_fixed + g_stats.navmesh_vertices_fixed;

			logger::info("[Disabled Reference Integrity Fix] Done: {} cells | {} refs | {} fix(es)  "
				"(below30K:{} initDis:{} deleted:{} navVerts:{})",
				g_stats.cells_processed, g_stats.total_refs_checked, total,
				g_stats.refs_below_min, g_stats.refs_initially_disabled_fixed,
				g_stats.deleted_refs_fixed, g_stats.navmesh_vertices_fixed);

			for (const auto& [wsID, ws] : g_worldspace_stats) {
				if (ws.refs_fixed == 0 && ws.navmesh_vertices_fixed == 0) continue;
				const auto navStr = ws.navmesh_vertices_fixed > 0
					? std::format(" navVerts:{}", ws.navmesh_vertices_fixed)
					: std::string{};
				logger::info("  {} (0x{:08X}): {} cells | {} fixes  (below30K:{} initDis:{} deleted:{}{})",
					ws.name, wsID, ws.cells, ws.refs_fixed,
					ws.below_min_fixed, ws.init_disabled_fixed, ws.deleted_fixed, navStr);
			}

			if (total == 0)
				logger::info("[Disabled Reference Integrity Fix] Worldspace is clean, no objects below -30K.");

			logger::info("[hooks] initSeen:{} initFixedPreLive:{} initSkip(3d:{} attached:{} refsLoaded:{}) load3dGated:{} | diag(cairOk:{} excl:{}) | fallbackCells:{} fallbackRefFixes:{} navmeshEventCells:{} navmeshEventVerts:{}",
				g_stats.hook_init_seen,
				g_stats.hook_init_fixed_pre_live,
				g_stats.hook_init_skipped_has3d,
				g_stats.hook_init_skipped_cell_attached,
				g_stats.hook_init_skipped_refs_fully_loaded,
				g_stats.hook_load3d_gated,
				g_stats.hook_init_cair_z_ok,
				g_stats.hook_init_excluded,
				g_stats.fallback_event_cells_fixed,
				g_stats.fallback_event_refs_fixed,
				g_stats.fallback_event_navmesh_cells_fixed,
				g_stats.fallback_event_navmesh_vertices_fixed);
		}
	}

	void LogHookInstrumentation(const char* a_reason)
	{
		if (!ENABLE_LOGGING) return;
		logger::info("[hooks:{}] initSeen:{} initFixedPreLive:{} initSkip(3d:{} attached:{} refsLoaded:{}) load3dGated:{} | diag(cairOk:{} excl:{}) | fallbackCells:{} fallbackRefFixes:{} navmeshEventCells:{} navmeshEventVerts:{}",
			a_reason ? a_reason : "snapshot",
			g_stats.hook_init_seen,
			g_stats.hook_init_fixed_pre_live,
			g_stats.hook_init_skipped_has3d,
			g_stats.hook_init_skipped_cell_attached,
			g_stats.hook_init_skipped_refs_fully_loaded,
			g_stats.hook_load3d_gated,
			g_stats.hook_init_cair_z_ok,
			g_stats.hook_init_excluded,
			g_stats.fallback_event_cells_fixed,
			g_stats.fallback_event_refs_fixed,
			g_stats.fallback_event_navmesh_cells_fixed,
			g_stats.fallback_event_navmesh_vertices_fixed);
	}

	void MaybeLogHookInstrumentation(const char* a_reason, uint64_t a_intervalMs)
	{
		if (!ENABLE_LOGGING) return;
		const uint64_t now  = static_cast<uint64_t>(::GetTickCount64());
		uint64_t last = g_last_hook_log_ms.load(std::memory_order_relaxed);
		if (now - last < a_intervalMs) return;

		if (g_last_hook_log_ms.compare_exchange_strong(last, now, std::memory_order_relaxed)) {
			LogHookInstrumentation(a_reason);
		}
	}
}
