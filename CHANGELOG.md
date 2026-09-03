# Changelog

## 1.4.0

### Fixed
- **The plugin no longer moves vanilla quest items.** A reference that ships
  "initially disabled" with no enable parent is the engine's normal way of saying
  "a quest or script switches this on later" - vanilla favor-quest items are exactly
  this shape. Parking one at the Z floor and pinning it to an enable-state parent
  stranded it for good and could make the quest uncompletable. The initially-disabled
  rule now only applies to references that a *later* plugin actually overrode, which
  is the case this plugin exists to normalize (a mod deliberately hiding an object).
  Seen in the wild on `Amulet of Arkay` (0x000AB85C) and `Queen Freydis's Sword`
  (0x000AB873), both dragged from their proper positions down to -30000.
  This generalizes the one-off hardcoded exclusion added in 1.3.1.
- **`early_fix_on_load3d` is now in the shipped INI.** The setting existed and
  defaulted to `true`, but was undocumented and absent from the INI, so users hitting
  a freeze had no way to turn off the plugin's most invasive subsystem (the
  InitItemImpl / Load3D vtable hooks) short of uninstalling.

### Changed
- **The per-cell pass no longer holds an engine lock while it works.**
  `TESObjectCELL::ForEachReference` holds the cell's `BSSpinLock` for the entire walk.
  The corrections run inside that callback call back into the engine (`Disable()`,
  `Update3DPosition()`, `extraList.Add()`) and write log lines to disk, so a spin lock
  was held across engine calls and file I/O on every cell load: any other thread
  needing that cell's reference list burned CPU waiting, and the engine code being
  called could itself reach the list being iterated. The pass now copies the reference
  pointers out under the lock and does the work with no engine lock held.
- **The periodic hook heartbeat moved to debug level.** It fired every 5 seconds for
  the whole session, so at the default log level it was always the last line in the
  log whenever the game stopped for any reason - which repeatedly got the plugin
  blamed for unrelated crashes. Set `log_level = 4` to get it back. The milestone
  snapshots (data-loaded, post-load-game, new-game) carry the same counters and are
  still logged at the default level.
- The hook counters now report `authorDis:`, the number of references skipped by the
  new quest-safety rule.
- Removed a duplicated `[hooks]` snapshot that was emitted immediately before every
  `[hooks:post-load-game]` line.

### Notes
- The INI documented dash-chaining for `excludemod` (`A.esp - B.esp`), which never
  worked and must not: plugin names routinely contain " - " (`Lux - Embers XD
  patch.esp`). The documentation now says commas only, which is what the parser does.

## 1.3.1

- Reverted an internal change from 1.3.0 that caused crashes and freezes for some users.
- Fixed a bug where two risky options could turn themselves on if the INI file was
  missing or outdated — a likely cause of random crashes with no crash log.
- The mod no longer touches objects that other mods hide on purpose (quest items etc.),
  which could break quests.
- **Updating from 1.2.x?** Replace your old INI file with the new one.

## 1.3.0

### Fixed
- **Micro-freezes / stutter while logging is enabled.** The file logger flushed to disk
  on every `info` line (the default log level). During a cell load that corrects many
  references this meant hundreds of blocking disk writes. The logger now flushes only on
  `warn` and above; `info` lines are buffered. Warnings, errors and critical messages
  (the ones relevant to crash diagnosis) are still flushed immediately.
- **Removed a heavy, re-entrant engine call from the loading-thread hooks.** The early
  `InitItemImpl` / `Load3D` path clamped a reference's Z by calling the full
  `TESObjectREFR::SetPosition()` while the engine was still initializing that reference.
  Since the hook only runs on pre-live references (cell not attached, 3D not loaded), the
  record position is now written directly, matching the runtime correction path. This
  removes a plausible cause of crashes during fast travel and cell transitions.

### Changed
- **Safer default configuration.** In the shipped INI, `fix_navmeshes` and
  `include_deleted_refs` now default to `false`. These are the two most invasive
  operations (runtime navmesh vertex edits and deleted-reference recovery); they can be
  re-enabled explicitly by users who need them. Reference Z-correction (`fix_references`)
  remains enabled by default.
- **Performance:** the per-reference "excluded mod" check no longer allocates a string on
  every `Load3D` / `InitItemImpl` call when no mods are excluded (the default).
- Plugin version bumped to 1.3.0 and the in-plugin description updated.

### Removed
- Removed the misleading `try/catch` around the navmesh pass. It caught only C++
  exceptions, never the access violations it appeared to guard against, so it provided no
  real protection. Null checks on the navmesh data are kept.
- Removed dead code: the unused `g_plugin_enabled` flag, the unused bundled `MinHook.h`
  header, and three unused include directories in the build script.

### Notes
- No change to the reference-fixing behavior itself: references mis-placed below the
  -30,000 Z boundary are still set initially-disabled, clamped, and given an opposite
  enable-state parent on the player.
