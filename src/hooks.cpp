#include "pch.h"
#include "hooks.h"
#include "config.h"
#include "patcher.h"
#include "utils.h"

using namespace DisabledReferenceIntegrityFix::Config;

namespace DisabledReferenceIntegrityFix
{
	namespace
	{
		bool IsExcludedFast(RE::TESObjectREFR* a_ref)
		{
			if (!a_ref) return true;
			if (IsHardcodedExcludedRef(a_ref)) return true;

			if (a_ref->GetFormType() == RE::FormType::ActorCharacter) return true;
			if (a_ref->IsPersistent()) return true;
			if (a_ref->HasQuestObject()) return true;
			if (a_ref->extraList.HasType<RE::ExtraLinkedRef>()) return true;

			if (a_ref->extraList.HasType<RE::ExtraEnableStateParent>()) return true;

			if (IsFormFromExcludedMod(a_ref)) return true;

			if (EXCLUDED_FORMS.contains(a_ref->GetFormID())) return true;
			if (const auto* base = a_ref->GetBaseObject()) {
				if (EXCLUDED_FORMS.contains(base->GetFormID())) return true;
				if (IsFormFromExcludedMod(base)) return true;
			}

			return false;
		}

		bool ShouldPatchCell(RE::TESObjectREFR* a_ref)
		{
			if (!a_ref) return false;
			auto* cell = a_ref->GetParentCell();
			if (!cell) return true;
			return cell->IsInteriorCell() ? PATCH_INTERIOR : PATCH_EXTERIOR;
		}

		bool PrepareReferenceForEarlyLoad(RE::TESObjectREFR* a_ref, bool* a_touched = nullptr)
		{
			if (a_touched) {
				*a_touched = false;
			}

			if (!a_ref || !FIX_REFERENCES) return false;
			if (!ShouldPatchCell(a_ref)) return false;

			if (auto* cell = a_ref->GetParentCell()) {
				if (cell->IsAttached()) return false;
				if (const auto* loaded = cell->GetRuntimeData().loadedData) {
					if (loaded->refsFullyLoaded) return false;
				}
			}

			if (IsExcludedFast(a_ref)) {
				g_hook_stats.init_excluded.fetch_add(1, std::memory_order_relaxed);
				return false;
			}

			const auto pos = a_ref->GetPosition();
			auto* base = a_ref->GetBaseObject();

			const bool canApplyInitDisabledRule = a_ref->IsInitiallyDisabled() &&
				!a_ref->IsPersistent() &&
				!a_ref->HasQuestObject();

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
					g_hook_stats.init_cair_z_ok.fetch_add(1, std::memory_order_relaxed);
				}
				AttachPlayerEnableParentOpposite(a_ref);
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

				if (PrepareReferenceForEarlyLoad(a_this)) {
					g_hook_stats.load3d_gated.fetch_add(1, std::memory_order_relaxed);
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

				g_hook_stats.init_seen.fetch_add(1, std::memory_order_relaxed);

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
					g_hook_stats.init_skipped_has3d.fetch_add(1, std::memory_order_relaxed);
					return;
				}
				if (cellAttached) {
					g_hook_stats.init_skipped_cell_attached.fetch_add(1, std::memory_order_relaxed);
					return;
				}
				if (refsFullyLoaded) {
					g_hook_stats.init_skipped_refs_fully_loaded.fetch_add(1, std::memory_order_relaxed);
					return;
				}

				bool touched = false;
				PrepareReferenceForEarlyLoad(a_this, &touched);
				if (touched) {
					g_hook_stats.init_fixed_pre_live.fetch_add(1, std::memory_order_relaxed);
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
