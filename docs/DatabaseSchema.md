# Database Schema

This document describes the schema/contracts used by the current source in `data/sql`.

## Database placement

### `acore_world`

Permanent definitions and calendar execution history:

| Table | Purpose |
|---|---|
| `lw_response_origin` | Scheduler capacity origin |
| `lw_invasion` | Top-level invasion/scheduler definition |
| `lw_invasion_stage` | Ordered invasion stages |
| `lw_stage_action` | Ordered stage-start actions |
| `lw_spawn_group` | Route-node anchored spawn definition |
| `lw_spawn_member` | Creature/GameObject composition |
| `lw_movement_path` / `lw_movement_node` / `lw_movement_profile` | Dense physical movement data |
| `lw_movement_node_action` | Physical movement-node actions, including GameObject opening |
| `lw_route_node` / `lw_route_segment` | Shared logical route graph |
| `lw_route_node_action` | Invasion/group semantic route-node actions |
| `lw_runtime_signal` | Reusable signal IDs/names |
| `lw_dialogue` / `lw_announcement` | Presentation definitions |
| `lw_creature_template` | Logical portable LW creature definitions |
| `lw_creature_template_map` | LW-managed logical-to-allocated creature entry mapping |
| `lw_creature_ability` | Data-driven abilities for logical LW creatures |
| `lw_traveling_event` / `lw_traveling_event_member` | Generic traveling-event definitions and arbitrary members |
| `lw_traveling_event_stop` / `lw_traveling_event_leg` | Physical stops and directional journey dialogue |
| `lw_traveling_camp_layout` / `lw_traveling_camp_layout_member` / `lw_traveling_camp_layout_prop` | Neutral reusable camp arrangements |
| `lw_traveling_camp_node_z_override` | Physical camp terrain corrections by placement-row ID |
| `lw_calendar_schedule` / `lw_calendar_rotation` / `lw_calendar_action` | Deterministic calendar rules/actions |
| `lw_calendar_execution` | Calendar occurrence/action de-duplication/history |

### `acore_characters`

| Table | Purpose |
|---|---|
| `lw_invasion_runtime` | Scheduler Available/Active/Cooldown state, cooldowns, counters |
| `lw_active_runtime` | Active invasion/stage timestamp persistence |

## Invasion stages

`lw_invasion_stage.completion_type`:

- `0` timer (`duration_seconds`)
- `1` runtime signal (`completion_target_id` = `lw_runtime_signal.id`)
- `2` objective — reserved/unimplemented
- `3` manual — reserved/unimplemented

## Stage actions

Actions execute immediately in `action_order` when the stage begins. `delay_seconds` is loaded but not currently deferred/scheduled.

| Type | Action | Target / parameters |
|---:|---|---|
| 1 | Spawn Group | `target_id=spawn_group_id` |
| 2 | Start Route Journey | `target_id=spawn_group_id`, `p1=start_route_node_id`, `p2=destination_route_node_id`, `p3=completion_signal_id` |
| 3 | Dialogue | `target_id=spawn_group_id`, `p1=dialogue_id`, `p2=speaker spawn_member_id`, `p3=target-policy bitmask` |
| 4 | World Announcement | `target_id=announcement_id`, `p1=scope`, `p2=scope_id`, `p3=faction` |
| 5 | Sound | `target_id=spawn_group_id`, `p1=SoundEntries ID`, `p2=source member`, `p3=playback mode` |
| 6 | Spell | `target_id=caster spawn_group_id`, `p1=spell_id`, `p2=caster member`, `p3=target mode` |
| 7 | Start Assault | `target_id=spawn_group_id`, `p1=radius`, `p2=reacquire ms`, `p3=target-policy bitmask`, `p4=required assault-center route node` |
| 8 | Watch Group Defeat | `target_id=spawn_group_id`, `p1=signal_id`, `p2=0 ANY / 1 ALL` |
| 9 | Start Garrison Restock | `target_id=spawn_group_id`, `p1=quiet seconds`, `p2=batch size`, `p3=refill interval seconds` |

Announcement scope: `0` global, `1` map, `2` zone, `3` area. Announcement faction: `0` everyone, `1` Alliance, `2` Horde. Sound mode: `0` positional, `1` direct. Scripted stage spell target mode currently supports `0` self.

Dialogue action `parameter3` target-policy bits are `1` quest givers, `2` vendors, `4` flight masters. Assault target-policy bits currently document `1` quest givers and `2` vendors.

## Spawn groups and members

`lw_spawn_group.route_node_id` is the stable spatial anchor. Raw XYZ is not invasion-facing data.

`lw_spawn_member.entity_type`: `1` Creature, `2` GameObject.

Creature members may use `entity_entry` directly or a logical `lw_template_id`. `level_override=0` preserves normal template level behavior.

Tactical roles: `0` Default, `1` Commander, `2` Protector, `3` Melee DPS, `4` Ranged DPS, `5` Healer, `6` Support. Roles influence LW formation placement, not a generic combat AI replacement.

## Logical LW creature templates

`lw_creature_template` defines a portable creature based on an existing AzerothCore creature and optional overrides. `lw_creature_template_map` is managed by LW and persistently owns the dynamically allocated `creature_template.entry` for each logical ID.

The materializer intentionally does not inherit SmartAI/ScriptName/vendor/quest/trainer/gossip/loot/gold/permanent movement from the base creature. Level is controlled per spawn member. Disabled logical definitions retire but reserve their allocation.

Materialization happens at startup before AzerothCore loads creature templates; changing these definitions requires a worldserver restart.

## Creature abilities

`lw_creature_ability` attaches spells to `lw_template_id`.

Target types:

- `0` self
- `1` lowest-health friendly LW creature in same runtime
- `2` random qualifying friendly LW creature in same runtime
- `3` current hostile victim

Rows also define priority, health threshold, cooldown, range, combat requirement, and enabled state.

## Movement and route data

`lw_movement_path`/`node` store dense physical geometry. `lw_route_node`/`segment` expose stable semantic graph endpoints to consumers. Segments can be traversed in either direction.

`lw_route_node_action` is invasion + spawn-group scoped and supports semantic dialogue, announcement, and sound actions at route anchors.

`lw_movement_node_action` is attached to dense physical nodes. Current movement code includes action type `4` for opening a GameObject by **database GUID**, with `parameter1` as movement hold time in milliseconds. The in-game helper `.lw route action add open-go ...` authors this action at the nearest movement node.

## Traveling-event schema contract

The traveling engine is neutral rather than salesman-specific. `traversal_mode`: `0` LOOP, `1` PING_PONG, `2` ONE_WAY. `auto_start=1` starts persistent events on worldserver startup; calendar-controlled events normally use `0`.

`lw_traveling_event_member` defines arbitrary Creature members using stable `member_key` values, member order, exactly one leader, and optional `vendor_while_camped`.

Stops contain physical location/camp state only. Directional departure and arrival dialogue is stored in `lw_traveling_event_leg`, keyed by `from_stop_id` and `to_stop_id`. Physical movement still uses the shared route graph.

Camp layouts place members by `member_key` and GameObjects by prop rows. Z overrides target placement row IDs: `1` member placement, `2` prop placement.

Existing development databases using the old prototype schema should run `manual/022_rebuild_traveling_event_generic.sql` and then reapply prebuilt `901`.

## Calendar tables

`lw_calendar_schedule.recurrence_type`:

- `1` one-time
- `2` annual
- `3` monthly weekday on/before anchor day
- `4` monthly weekday on/after anchor day

`weekday`: `0` Sunday through `6` Saturday. `catchup_seconds` defines how late an overdue action may still run. `rotation_offset` phase-shifts the occurrence ordinal.

`lw_calendar_rotation` provides ordered rows with optional `target_id_override`, generic `value`, and label.

`lw_calendar_action.target_type`:

- `1` start invasion
- `2` start traveling event
- `3` stop traveling event

Actions can offset the occurrence by days/minutes and optionally use the selected rotation target. `parameter1`/`parameter2` are present but currently unused by implemented target types.

`lw_calendar_execution` records successful `(action_id, occurrence_key)` executions so periodic evaluation/restarts do not repeat them.

## Runtime persistence caveat

`lw_active_runtime` persists invasion/stage timing state, not a complete reconstruction of every temporary world entity/GUID and subsystem state. It is not full worldserver restart recovery.