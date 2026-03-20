#include "pch.h"
#include "hooks.h"
#include "config.h"
#include "patcher.h"

using namespace DisabledReferenceIntegrityFix::Config;

namespace DisabledReferenceIntegrityFix
{
	namespace
	{
		const char* FormSourceFile(const RE::TESForm* a_form)
		{
			if (!a_form) return "<null>";
			if (const auto* file = a_form->GetFile(0)) return file->fileName;
			return "<unknown>";
		}

		void LogRefFix(const char* a_stage, RE::TESObjectREFR* a_ref, float a_oldZ, float a_newZ, const char* a_reason)
		{
			if (!ENABLE_LOGGING || !a_ref) return;

			auto* cell = a_ref->GetParentCell();
			const auto cellID = cell ? cell->GetFormID() : 0;
			auto* base = a_ref->GetBaseObject();
			const auto baseID = base ? base->GetFormID() : 0;
			const auto cellType = (cell && cell->IsInteriorCell()) ? "int" : "ext";
			int32_t cellX = 0;
			int32_t cellY = 0;
			if (cell) {
				if (auto* ext = cell->GetCoordinates()) {
					cellX = ext->cellX;
					cellY = ext->cellY;
				}
			}

			logger::info("[ref-fix] stage={} reason={} ref=0x{:08X} oldZ={:.3f} newZ={:.3f} cell=0x{:08X} cellType={} cellXY=({}, {}) base=0x{:08X} srcRef={} srcBase={}",
				a_stage ? a_stage : "?",
				a_reason ? a_reason : "?",
				a_ref->GetFormID(),
				a_oldZ,
				a_newZ,
				cellID,
				cellType,
				cellX,
				cellY,
				baseID,
				FormSourceFile(a_ref),
				FormSourceFile(base));
		}
		bool IsMarkerBase(RE::TESForm* a_form)
		{
			if (!a_form) return false;
			if (auto* obj = a_form->As<RE::TESObject>()) {
				return obj->IsMarker();
			}
			return false;
		}

		bool IsBlacklistedMaster(std::string_view a_fileName)
		{
			if (a_fileName.empty()) return false;
			std::string lower(a_fileName);
			for (auto& c : lower)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

			return lower == "skyrim.esm" ||
				lower == "update.esm" ||
				lower == "dawnguard.esm" ||
				lower == "hearthfires.esm" ||
				lower == "dragonborn.esm" ||
				lower == "dyndolod.esp";
		}

		bool IsExcludedFast(RE::TESObjectREFR* a_ref)
		{
			if (!a_ref) return true;

			if (a_ref->IsPersistent()) return true;
			if (a_ref->HasQuestObject()) return true;
			if (a_ref->extraList.HasType<RE::ExtraLinkedRef>()) return true;

			if (a_ref->extraList.HasType<RE::ExtraEnableStateParent>()) return true;

			if (const auto* lastFile = a_ref->GetFile(-1)) {
				if (IsBlacklistedMaster(lastFile->fileName)) return true;
				std::string lowerName(lastFile->fileName);
				for (auto& c : lowerName)
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				if (EXCLUDED_MODS.contains(lowerName)) return true;
			}

			if (EXCLUDED_FORMS.contains(a_ref->GetFormID())) return true;
			if (const auto* base = a_ref->GetBaseObject()) {
				if (EXCLUDED_FORMS.contains(base->GetFormID())) return true;
			}

			return false;
		}

		bool ShouldPatchCell(RE::TESObjectREFR* a_ref)
		{
			if (!a_ref) return false;
			auto* cell = a_ref->GetParentCell();
			if (!cell) return true;
			if (cell->IsInteriorCell()) return PATCH_INTERIOR;
			return PATCH_EXTERIOR;
		}

		bool PrepareReferenceForEarlyLoad(RE::TESObjectREFR* a_ref, bool* a_touched = nullptr)
		{
			if (a_touched) {
				*a_touched = false;
			}

			if (!a_ref || !FIX_REFERENCES) return false;
			if (!ShouldPatchCell(a_ref)) return false;

			const auto pos = a_ref->GetPosition();
			auto* base = a_ref->GetBaseObject();

			const bool canApplyInitDisabledRule = a_ref->IsInitiallyDisabled() &&
				!a_ref->IsPersistent() &&
				!a_ref->HasQuestObject() &&
				!a_ref->extraList.HasType<RE::ExtraEnableStateParent>();

			if (canApplyInitDisabledRule) {
				if (!base || IsMarkerBase(base)) return false;
				if (std::fabs(pos.z - Z_FLOOR) > Z_EPSILON) {
					auto fixedPos = pos;
					fixedPos.z = Z_FLOOR;
					a_ref->SetPosition(fixedPos);
					if (a_touched) {
						*a_touched = true;
					}
					LogRefFix("init", a_ref, pos.z, fixedPos.z, "init_disabled_rule_clamp");
				} else {
					g_stats.hook_init_cair_z_ok++;
				}
				return false;
			}

			if (pos.z <= Z_FLOOR && !a_ref->IsInitiallyDisabled()) {
				if (!base || IsMarkerBase(base)) return false;
				a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInitiallyDisabled;
				if (a_touched) {
					*a_touched = true;
				}
				LogRefFix("init", a_ref, pos.z, pos.z, "below_floor_gate");
				if (VERBOSE_LOGGING && ENABLE_LOGGING)
					logger::trace("[pre3d] 0x{:08X} below floor ({:.0f}), 3D gated", a_ref->GetFormID(), pos.z);
				return true;
			}

			if (INCLUDE_DELETED && a_ref->IsDeleted()) {
				if (IsExcludedFast(a_ref)) return false;
				if (!base || IsMarkerBase(base)) return false;
				const float oldZ = pos.z;
				a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInitiallyDisabled;
				if (a_touched) {
					*a_touched = true;
				}
				LogRefFix("init", a_ref, oldZ, oldZ, "deleted_gate");
				if (VERBOSE_LOGGING && ENABLE_LOGGING)
					logger::trace("[pre3d] 0x{:08X} deleted ref gated before 3D load", a_ref->GetFormID());
				return true;
			}

			return false;
		}

		class TESObjectREFR_Load3DHook
		{
		public:
			static void Install()
			{
				REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_TESObjectREFR[0] };
				_Load3D = vtbl.write_vfunc(0x6A, Thunk);
			}

		private:
			static RE::NiAVObject* Thunk(RE::TESObjectREFR* a_this, bool a_backgroundLoading)
			{
				if (!a_this || !EARLY_FIX_ON_LOAD3D) {
					return _Load3D(a_this, a_backgroundLoading);
				}

				bool touched = false;
				if (PrepareReferenceForEarlyLoad(a_this, &touched)) {
					g_stats.hook_load3d_gated++;
					LogRefFix("load3d", a_this, a_this->GetPosition().z, a_this->GetPosition().z, "load3d_gate");
					MaybeLogHookInstrumentation("live", 5000);
					return nullptr;
				}

				MaybeLogHookInstrumentation("live", 5000);

				return _Load3D(a_this, a_backgroundLoading);
			}

			static inline REL::Relocation<decltype(Thunk)> _Load3D;
		};

		class TESObjectREFR_InitItemImplHook
		{
		public:
			static void Install()
			{
				REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_TESObjectREFR[0] };
				_InitItemImpl = vtbl.write_vfunc(0x13, Thunk);
			}

		private:
			static void Thunk(RE::TESObjectREFR* a_this)
			{
				_InitItemImpl(a_this);

				if (!a_this || !EARLY_FIX_ON_LOAD3D || !FIX_REFERENCES) {
					return;
				}

				g_stats.hook_init_seen++;

				const bool has3D = a_this->Is3DLoaded();
				auto*      cell  = a_this->GetParentCell();

				bool cellAttached = false;
				bool refsFullyLoaded = false;
				if (cell) {
					cellAttached = cell->IsAttached();
					if (auto* loaded = cell->GetRuntimeData().loadedData) {
						refsFullyLoaded = loaded->refsFullyLoaded;
					}
				}

				if (has3D) {
					g_stats.hook_init_skipped_has3d++;
					return;
				}
				if (cellAttached) {
					g_stats.hook_init_skipped_cell_attached++;
				}
				if (refsFullyLoaded) {
					g_stats.hook_init_skipped_refs_fully_loaded++;
				}

				bool touched = false;
				PrepareReferenceForEarlyLoad(a_this, &touched);
				if (touched) {
					g_stats.hook_init_fixed_pre_live++;
				}

				MaybeLogHookInstrumentation("live", 5000);
			}

			static inline REL::Relocation<decltype(Thunk)> _InitItemImpl;
		};
	}

	void InstallRuntimeHooks()
	{
		if (!EARLY_FIX_ON_LOAD3D) {
			if (ENABLE_LOGGING)
				logger::info("[Disabled Reference Integrity Fix] Early Load3D hook disabled by config");
			return;
		}

		try {
			TESObjectREFR_InitItemImplHook::Install();
			TESObjectREFR_Load3DHook::Install();
			if (ENABLE_LOGGING)
				logger::info("[Disabled Reference Integrity Fix] Early reference hooks installed (InitItemImpl + Load3D)");
		} catch (const std::exception& e) {
			logger::error("[Disabled Reference Integrity Fix] Failed to install Load3D hook: {}", e.what());
		} catch (...) {
			logger::error("[Disabled Reference Integrity Fix] Failed to install Load3D hook: unknown exception");
		}
	}
}
