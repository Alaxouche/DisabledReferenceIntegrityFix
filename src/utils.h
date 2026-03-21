#pragma once

#include "config.h"

namespace DisabledReferenceIntegrityFix
{
	inline const char* FormSourceFile(const RE::TESForm* a_form)
	{
		if (!a_form) return "<null>";
		if (const auto* file = a_form->GetFile(0)) return file->fileName;
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

	inline bool IsBlacklistedMaster(std::string_view a_fileName)
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
			lower == "dyndolod.esp" ||
			lower == "legacyofthedragonborn.esm";
	}

	inline bool IsModExcludedByName(std::string_view a_fileName)
	{
		if (a_fileName.empty()) return false;
		std::string lowerName(a_fileName);
		for (auto& c : lowerName)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		if (IsBlacklistedMaster(lowerName)) return true;
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
}
