# Invasion Data Reference

This file describes the current invasion-facing data model. Physical coordinates and dense movement paths belong to the route layer (`801_routes.sql`), not to individual invasion prebuilts.

## Spatial ownership

`801_routes.sql` owns:

- `lw_route_node` semantic locations;
- `lw_route_segment` graph edges;
- route-owned `lw_movement_path` / `lw_movement_node` rows.

An invasion prebuilt owns behavior and composition. It references stable route-node IDs but should not duplicate route-owned XYZ or movement-node data.

## Spawn groups

```sql
INSERT INTO lw_spawn_group
    (id, name, route_node_id, spawn_radius, enabled)
VALUES
    (100, 'Example Force', 140, 10, 1);
```

`route_node_id` is the group's semantic spawn anchor. The route node supplies map, X/Y/Z, and orientation. `spawn_radius` spreads individual spawned entities around the anchor.

## Spawn members

`lw_spawn_member` continues to define entity composition, count, level override, and tactical role. Tactical roles affect formation positioning but do not replace native AzerothCore combat AI.

## Stage action type 2 — Start Route Journey

```text
target_id  = spawn_group_id
parameter1 = start_route_node_id
parameter2 = destination_route_node_id
parameter3 = completion_signal_id (0 = none)
```

The route graph resolves the physical segments and route-owned movement paths between the two route nodes. Invasion SQL never references a `movement_path_id` directly.

Example:

```sql
-- Move spawn group 106 from Stormwind_Response_Force_Spawn (140)
-- to Sentinel_Hill_Tower (70), then emit signal 103.
(..., 2, 106, 140, 70, 103, ...)
```

## Route-node actions

`lw_route_node_action` provides invasion-specific behavior at semantic route anchors.

```text
id
invasion_id
spawn_group_id
route_node_id
action_order
action_type
target_id
parameter1
parameter2
parameter3
enabled
comment
```

Supported action types:

### 1 — Dialogue

```text
target_id  = dialogue_id
parameter1 = speaker spawn_member_id (0 = first available creature)
```

### 2 — World Announcement

```text
target_id  = announcement_id
parameter1 = scope: 0 global, 1 map, 2 zone, 3 area
parameter2 = scope_id (0 derives map/zone from invasion where supported)
parameter3 = faction: 0 everyone, 1 Alliance, 2 Horde
```

### 3 — Sound

```text
target_id  = SoundEntries ID
parameter1 = source spawn_member_id (0 = first available creature)
parameter2 = playback mode: 0 positional, 1 direct
```

Actions are scoped to both invasion and spawn group, so a merchant or another invasion can traverse the same route node without inheriting the action.

A spawn group's own route node counts as reached immediately after successful spawn. While traveling, the commander is used as the group anchor when one exists; otherwise the first living creature is used. Actions fire when that anchor enters the route node's `arrival_radius`. Graph-node arrival is also reported explicitly at segment boundaries.

## Recommended prebuilt dependency order

1. `801_routes.sql`
2. invasion / stages
3. spawn groups
4. spawn members and templates
5. runtime signals / dialogue / announcements
6. stage actions
7. route-node actions

## Authoring rule

If an invasion needs to spawn somewhere, travel somewhere, or trigger something at a meaningful location, create/reuse a semantic `lw_route_node` and reference its stable ID. Use the automatic 5-yard route builder for the physical paths between those semantic anchors.

## Logical creature templates and abilities

For portable custom invasion creatures, `lw_spawn_member.lw_template_id` references `lw_creature_template.id`. LW owns the actual dynamically allocated AzerothCore creature entry through `lw_creature_template_map`. Optional `lw_creature_ability` rows attach combat spells to the logical template. Template materialization occurs at worldserver startup.

## Stage action types 7-9

### 7 — Start Assault

```text
target_id  = spawn_group_id
parameter1 = radius yards (0 = 40)
parameter2 = reacquire interval ms (0 = 2000, minimum 500)
parameter3 = target policy: bit 1 quest givers, bit 2 vendors
parameter4 = REQUIRED assault-center route_node_id
```

### 8 — Watch Group Defeat

```text
target_id  = spawn_group_id
parameter1 = runtime signal id
parameter2 = 0 ANY watched group defeated, 1 ALL watched groups defeated
```

Multiple rows may register groups into the same signal/mode watch.

### 9 — Start Garrison Restock

```text
target_id  = spawn_group_id
parameter1 = quiet period seconds (0 = 30)
parameter2 = maximum replacements per batch (0 = 5)
parameter3 = refill interval seconds (0 = 10)
```

The garrison must retain at least one living creature; the system does not regenerate a completely defeated group from zero.
