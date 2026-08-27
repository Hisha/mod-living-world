# Architecture

## Design boundary

LW separates reusable mechanics from authored world content.

- **C++** owns scheduling, runtime lifecycle, entity providers, movement, assault behavior, defeat watches, garrison restocking, signals, creature abilities, traveling-event lifecycle, calendar evaluation, failure handling, and cleanup.
- **SQL** owns invasion/event definitions, stages/actions, spawn compositions, logical creature templates, abilities, routes, route actions, dialogue, announcements, traveling stops/camps, and calendar schedules/rotations.
- **AzerothCore** owns normal combat/pathfinding/world rules and the base data from which LW derives creatures.

## Source ownership

The source tree enforces a one-way dependency boundary:

```text
Living World Core
    ↑
    ├── Invasions
    └── Travelers

Future: Hunts / Caravans / Dynamic Quests
    ↑
Living World Core
```

`src/core/` owns mechanics that can be reused by more than one gameplay system. `src/invasions/` owns invasion-only runtime behavior. `src/travelers/` owns persistent traveling-event lifecycle. New gameplay systems should consume core services rather than adding their behavior to core.

Subsystems can be disabled independently with `LivingWorld.Invasions.Enable` and `LivingWorld.Travelers.Enable`. `LivingWorld.Enable` remains the master switch.

## Major runtime flows

### Invasion

```text
InvasionScheduler
  -> selects eligible invasion
  -> InvasionRuntimeManager creates runtime
  -> runtime begins ordered stage
  -> stage actions execute in action_order
  -> stage waits for timer or runtime signal
  -> next stage
  -> completion / failure / timeout
  -> managers cancel + runtime entities clean up
  -> scheduler enters cooldown
```

A calendar action can also start an invasion directly through `InvasionRuntimeManager`; the invasion's own stages still own its lifecycle.

### Traveling world event

```text
TravelingEventManager
  -> spawn merchant + two pack mules at current stop
  -> materialize camp layout
  -> dwell while camped
  -> wait for configured daily travel window if necessary
  -> remove camp/vendor state
  -> graph-route caravan to next stop
  -> materialize next camp
  -> repeat
```

Traveling events are intentionally separate from invasion runtimes.

### Calendar

```text
CalendarManager
  -> evaluate server-local recurrence
  -> resolve optional rotation row
  -> apply action day/minute offset
  -> honor catch-up window
  -> suppress duplicate occurrence/action execution
  -> start invasion OR start/stop traveling event
```

## Definition layer

`LivingWorldDataMgr` loads invasion-facing definitions including response origins, invasions/stages/actions, spawn groups/members, movement/route data, signals, dialogue, announcements, and route/movement-node actions. `LwCreatureTemplateManager` materializes logical LW creature definitions at startup. `CreatureAbilityManager`, `TravelingEventManager`, and `CalendarManager` load their own subsystem data.

Definitions contain logical IDs rather than runtime GUIDs.

## Scheduler and invasion runtime

`InvasionScheduler` persists per-invasion Available/Active/Cooldown state in `acore_characters.lw_invasion_runtime` and enforces global, map, and response-origin capacity. Random selection considers enabled definitions with `allow_random_start=1` and their selection weights.

`InvasionRuntimeManager` owns active `InvasionRuntime` objects. Implemented stage completion types are:

- `0` Timer
- `1` Runtime Signal

Schema values `2` Objective and `3` Manual remain reserved/unimplemented.

A runtime can end by completing all stages, explicit framework failure, hard `maximum_runtime_seconds` timeout, or emergency abort.

## Stage actions

Implemented stage actions are:

1. Spawn Group
2. Start Route Journey
3. Dialogue
4. World Announcement
5. Sound
6. Spell
7. Start Assault
8. Watch Group Defeat
9. Start Garrison Restock

Actions execute immediately in `action_order`. `delay_seconds` is loaded but is not currently scheduled as a deferred action.

## Runtime entities and LW creatures

Spawn groups create temporary runtime entity groups. Providers support Creature and GameObject entities. Creature members may reference either a normal `entity_entry` or a logical `lw_template_id`.

Logical LW creature templates are materialized into dynamically allocated `creature_template` entries at worldserver startup. They can override selected presentation/combat-template properties while avoiding hard-coded custom entry IDs in prebuilt invasion SQL. Template-definition changes require a worldserver restart; `.lw reload` does not rematerialize them.

Data-driven `lw_creature_ability` rows attach reusable spell behavior to logical templates. Current target modes are self, lowest-health friendly LW creature in the same runtime, random qualifying friendly LW creature, and current hostile victim.

## Movement and route graph

`MovementController` moves living creatures using AzerothCore PathGenerator/MMAP. Shared `lw_route_node`/`lw_route_segment` data forms a reusable bidirectional graph above dense movement paths.

The controller supports route chaining, walk/run mode, role-aware formations, combat interruption/resume, casualty-tolerant regrouping, strategic final-objective arrival, completion signals, route-node actions, and active-force wipe detection.

The in-game builder records shared road geometry at approximately 5-yard spacing. Published route-node IDs are stable consumer-facing anchors.

## Assault, defeat watch, and garrison behavior

`AssaultManager` can release a runtime group into an assault centered on a required route node, periodically acquiring eligible targets within a radius. Target policy can include quest givers and vendors. The manager can temporarily normalize/override world defenders for the assault and restores those overrides when the runtime is released.

`GroupDefeatWatcher` watches one or more authored spawn groups and emits a runtime signal when ANY or ALL watched groups are defeated.

`InvasionSpawnManager` can maintain a surviving garrison up to its authored member counts after movement ends and a configurable quiet period passes. A fully defeated garrison does not regenerate from zero.

## Traveling events and camps

The current traveling-event implementation uses an all-creature caravan: the legacy `leader_entry` column is interpreted as the merchant/route owner and `wagon_entry` as the pack-mule creature entry. Two pack mules are currently hard-coded by the runtime. Older wagon/GameObject offset columns remain compatibility baggage and are ignored by the current manager.

Stops use route-node IDs and optional camp-layout IDs. Camp layouts position the merchant, two mules, and GameObject props relative to route-node orientation. Per-node Z overrides correct local terrain placement. Departures are restricted by the configurable server-local travel window.

## Calendar engine

`CalendarManager` supports one-time, annual, monthly weekday-on/before, and monthly weekday-on/after recurrences. Schedules may have rotations, action offsets, and catch-up windows. Execution records prevent the same action/occurrence from firing repeatedly after evaluation/restart.

Current calendar target types are start invasion, start traveling event, and stop traveling event.
