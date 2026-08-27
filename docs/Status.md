# Current Status

**Version reported by module:** `0.3.0-dev`

The project has moved well beyond the original scheduler/runtime baseline. The current repository contains a functioning invasion framework, a substantial Westfall invasion, reusable route tooling, an LW-owned creature/ability layer, a traveling merchant/camp framework, and the first generic calendar/rotation engine.

## Implemented and represented in the current codebase

### Invasion/runtime

- weighted random invasion scheduling, cooldowns, global/map/response-origin capacity;
- persisted scheduler state and active stage timestamps;
- timer and runtime-signal stage completion;
- nine stage action types: spawn, route journey, dialogue, announcement, sound, spell, assault, defeat watch, and garrison restock;
- Creature/GameObject runtime groups and cleanup;
- hard runtime timeout and active movement force-wipe failure;
- enable/disable, trigger, drain/resume, reload, abort, status, signals, and version administration.

### Creature/combat layer

- logical LW creature templates with persistent dynamically allocated AzerothCore entries;
- template overrides for name/subname/faction/rank/modifiers/class;
- per-spawn level overrides and tactical roles;
- data-driven creature abilities with cooldown, combat/range/health gates, priority, and four target modes;
- assault target acquisition centered on a route node;
- group-defeat signal watches;
- quiet-period garrison restocking without resurrection from total defeat.

### Routes/movement

- stable route-node IDs and reusable bidirectional route segments;
- automatic graph journey resolution;
- dense 5-yard in-game route recording;
- route testing and SQL export (segment, journey, complete network);
- route-node semantic dialogue/announcement/sound actions;
- route GameObject open actions for doors/gates;
- combat pause/resume, formation movement, casualty-tolerant regrouping, and final-objective arrival semantics.

### Traveling world events

- non-invasion traveling-event manager;
- merchant plus two pack-mule caravan runtime;
- route-node stops and route-graph travel;
- reusable oriented camp layouts with GameObject props;
- per-route-node Z correction overrides;
- merchant vendor availability while camped;
- configurable server-local daily travel window;
- debug camp spawn/despawn plus start/stop/status commands;
- shipped Traveling Salesman prebuilt content.

### Calendar/rotation

- one-time and annual schedules;
- monthly weekday on/before or on/after an anchor day;
- day/minute action offsets;
- server-local scheduling;
- catch-up windows after downtime;
- execution de-duplication by action/occurrence;
- rotation rows with target overrides;
- actions to start invasions or start/stop traveling events;
- `.lw calendar status` and `.lw calendar reload`.

## Current authored content

`data/sql/db-world/prebuilt/900_defias_westfall_invasion.sql` is now the real Defias Westfall invasion content and uses the newer combat/runtime capabilities. `901_traveling_sales_wagon.sql` is a developed Traveling Salesman event with merchant stock, pack mules, multiple camps, camp props, terrain corrections, and a route loop.

## Important limitations

- Objective and Manual stage completion remain reserved/unimplemented.
- `lw_stage_action.delay_seconds` is not executed as a deferred action.
- Movement-profile speed multipliers and stealth are loaded but not applied by movement execution.
- Spell stage actions currently support self-cast only; creature abilities provide broader combat targeting separately.
- Runtime entity GUID/world state is not a complete restart-recovery system.
- Playerbot-specific invasion awareness/recruitment remains unimplemented even though `LivingWorld.Playerbots.Enable` exists.
- No generalized player contribution/reward/achievement system exists.
- Cross-map route transport edges (boat/zeppelin/portal transitions) are not implemented.
- Traveling-event data still carries prototype-era column names; the current all-creature caravan runtime interprets them differently from the original schema comments.

## Known repository issue found during this documentation audit

The clean-install traveling schema defines `lw_traveling_event_stop.camp_layout_id` in `018_lw_traveling_world_event.sql`, matching the runtime loader and the shipped traveling-salesman prebuilt. Camp layout definitions are created by `019_lw_traveling_camp_layout.sql`.

The same area also retains stale comments in `018` describing the older Creature + mobile GameObject wagon architecture, while the current C++ uses `leader_entry` as the merchant and `wagon_entry` as the pack-mule Creature entry.
