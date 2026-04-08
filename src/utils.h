#pragma once

#include "config.h"

namespace DisabledReferenceIntegrityFix
{
	inline const char* FormSourceFile(const RE::TESForm* a_form)
	{
		if (!a_form) return "<null>";
		if (const auto* file = a_form->GetFile(-1)) return file->fileName;
		if (const auto* file = a_form->GetFile(0))  return file->fileName;
		return "<unknown>";
	}

	inline bool IsMarkerBase(RE::TESForm* a_form)
	{
		if (!a_form) return false;
		if (auto* obj = a_form->As<RE::TESObject>()) {
			return obj->IsMarker();
		}
		return false;
	}

	inline bool IsModExcludedByName(std::string_view a_fileName)
	{
		if (a_fileName.empty()) return false;
		std::string lowerName(a_fileName);
		for (auto& c : lowerName)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return Config::EXCLUDED_MODS.contains(lowerName);
	}

	inline bool IsFormFromExcludedMod(const RE::TESForm* a_form)
	{
		if (!a_form) return false;
		if (const auto* lastFile = a_form->GetFile(-1)) {
			if (IsModExcludedByName(lastFile->fileName)) return true;
		}
		if (const auto* file0 = a_form->GetFile(0)) {
			if (IsModExcludedByName(file0->fileName)) return true;
		}
		return false;
	}

	inline bool IsHardcodedExcludedRef(const RE::TESObjectREFR* a_ref)
	{
		if (!a_ref) return true;

		// Quest path guard: Double-Distilled Skooma reference in Windhelm theft quest.
		// This reference family may not carry reliable quest flags at runtime.
		constexpr RE::FormID kQuestRef_DoubleDistilledSkooma  = 0x0003F4BE;
		constexpr RE::FormID kQuestBase_DoubleDistilledSkooma = 0x0003F4BD;

		if (a_ref->GetFormID() == kQuestRef_DoubleDistilledSkooma) return true;
		if (const auto* base = a_ref->GetBaseObject()) {
			if (base->GetFormID() == kQuestBase_DoubleDistilledSkooma) return true;
		}

		return false;
	}
}
