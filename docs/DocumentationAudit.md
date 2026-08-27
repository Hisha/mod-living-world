# Documentation Audit — 2026-08-25

This audit compares the checked-in documentation with the current source and SQL in the supplied codebase.

## Major stale areas corrected

1. **Project scope was invasion-only.** The README/architecture now include traveling events and the calendar/rotation engine.
2. **Westfall was documented as future work.** `900_defias_westfall_invasion.sql` is now substantial real content and uses assault, defeat-watch, garrison, creature-template, and ability systems.
3. **Stage actions stopped at type 6.** Documentation now covers action types 7-9 and the required assault-center route node.
4. **Creature templates/abilities were missing.** Documentation now reflects logical LW templates, dynamic allocated entries, and data-driven combat abilities.
5. **Route export was still called future work in Status/Roadmap.** Segment/journey/network export is implemented and documented as complete.
6. **TravelingWorldEvents.md described the superseded mobile GameObject wagon proof of concept.** It now describes the current merchant + two pack-mule implementation, camps, vendor state, terrain corrections, and travel window.
7. **Calendar functionality was undocumented.** Added `Calendar.md` and linked it from the README.
8. **Testing command inventory was incomplete.** Updated for enable/disable, route GameObject actions, travel/camp commands, and calendar commands.

## Source/schema inconsistency discovered

The current clean-install SQL does not match the current traveling-event C++ contract:

- `TravelingEventManager::LoadDefinitions()` selects `camp_layout_id` from `lw_traveling_event_stop`.
- `901_traveling_sales_wagon.sql` inserts `camp_layout_id`.
- `018_lw_traveling_world_event.sql` does **not** define `camp_layout_id`.
- `019_lw_traveling_camp_layout.sql` creates layout tables but does **not** alter the stop table to add the column.

This should be fixed in the canonical pre-release schema. The documentation now calls it out rather than presenting the traveling schema as internally consistent.

## Prototype naming/comments still present in schema

`018_lw_traveling_world_event.sql` still documents the old mobile GameObject wagon design. Current C++ instead treats:

- `leader_entry` as merchant/route owner Creature;
- `wagon_entry` as pack-mule Creature entry;
- pack-mule count as two in code;
- old merchant/wagon-offset fields as ignored compatibility data.

Because this project is still pre-release, a future schema cleanup/rename would be preferable to permanently documenting misleading names.

## Documentation files intentionally retained

`02-scheduler-framework.md` and `03-runtime-engine.md` remain historical redirect notes. `BehaviorSystem.md` remains valid as a statement that there is no separate generic behavior DSL; concrete reusable behavior now lives in managers such as AssaultManager, CreatureAbilityManager, GroupDefeatWatcher, and garrison support.
