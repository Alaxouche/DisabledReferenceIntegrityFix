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
		if (Config::EXCLUDED_MODS.empty()) return false;
		if (const auto* lastFile = a_form->GetFile(-1)) {
			if (IsModExcludedByName(lastFile->fileName)) return true;
		}
		if (const auto* file0 = a_form->GetFile(0)) {
			if (IsModExcludedByName(file0->fileName)) return true;
		}
		return false;
	}

	// True when a plugin other than the one that first defined this record has
	// overridden it.
	//
	// A reference nobody overrode still carries the enable state its own author
	// gave it. An "initially disabled" reference with no enable parent is the
	// engine's normal way of saying "a quest or script switches this on later"
	// (vanilla favor-quest items are exactly this shape), so parking one at the Z
	// floor and pinning it to an enable-state parent strands it for good. Only a
	// reference that a *later* plugin edited was deliberately hidden by a mod, and
	// that is the case this plugin exists to normalize.
	inline bool IsOverriddenByLaterPlugin(const RE::TESForm* a_form)
	{
		if (!a_form) return false;
		const auto* array = a_form->sourceFiles.array;
		return array && array->size() > 1;
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
