# Traveling World Events

Traveling World Events are a generic non-invasion runtime. The Traveling Salesman is now only a SQL configuration of the same engine intended for future Darkmoon caravan use.

## Model

- `lw_traveling_event`: traversal mode, auto-start, enable state.
- `lw_traveling_event_member`: arbitrary Creature members identified by `member_key`; exactly one leader.
- `lw_traveling_event_stop`: physical route-node camp locations and dwell time only.
- `lw_traveling_event_leg`: directional departure/arrival dialogue between stop IDs.
- `lw_traveling_camp_layout_member`: member-key-based camp positions.
- `lw_traveling_camp_layout_prop`: GameObject positions.

Traversal modes are `0 LOOP`, `1 PING_PONG`, `2 ONE_WAY`. PING_PONG reuses the shared route network in reverse. Because dialogue is attached to directional legs rather than stops, reverse travel can use correct text without duplicating physical movement paths.

`auto_start=1` starts a persistent event at worldserver startup. Calendar-controlled events such as a future Darkmoon caravan should use `auto_start=0`.

Starting an event now materializes the complete initial camp immediately. If startup is outside the configured travel window, the full camp remains until departures are allowed.

Camp terrain overrides target placement row IDs: type `1` member placement, type `2` prop placement.

## Commands

```text
.lw travel start <eventId>
.lw travel stop <eventId>
.lw travel status
.lw travel <eventId> spawn <routeNodeId>
.lw travel <eventId> despawn <routeNodeId>
```

## Existing development database transition

Run `data/sql/db-world/manual/022_rebuild_traveling_event_generic.sql`, then re-run `data/sql/db-world/prebuilt/901_traveling_sales_wagon.sql`. Fresh installs use canonical 018/019/020 directly and do not need the manual transition.