# Calendar and Rotation Engine

`CalendarManager` is LW's generic server-local world-event scheduler. It is separate from `InvasionScheduler`: the invasion scheduler chooses random eligible invasions, while the calendar executes authored actions at deterministic date/time occurrences.

## Tables

- `lw_calendar_schedule` — recurrence rule, local time, catch-up window, rotation offset.
- `lw_calendar_rotation` — optional ordered rotation rows for a schedule.
- `lw_calendar_action` — one or more actions relative to each occurrence.
- `lw_calendar_execution` — de-duplication/history keyed by action and occurrence.

These tables currently live in `acore_world`.

## Recurrence types

| Value | Type | Meaning |
|---:|---|---|
| 1 | One-time | exact year/month/day |
| 2 | Annual | same month/day every year |
| 3 | Monthly weekday on/before | resolve a weekday on or before an anchor day |
| 4 | Monthly weekday on/after | resolve a weekday on or after an anchor day |

`weekday` follows `std::tm`: `0=Sunday` through `6=Saturday`.

Example: a monthly schedule with `day=1`, `weekday=1`, recurrence type `3` means **Monday on or before the first day of the month**. That can resolve into the previous calendar month.

## Action timing

Each action has `offset_days` and `offset_minutes` relative to the schedule occurrence. This allows one recurrence anchor to drive multiple phases. For example, a Monday anchor can have setup several days earlier and departure several days later without separate recurrence rules.

The engine evaluates on a short periodic timer and uses the worldserver machine's local time.

## Catch-up and duplicate protection

If an action is overdue but its age is no greater than `catchup_seconds`, LW attempts it. This allows short worldserver outages/restarts without silently losing a scheduled event.

After successful execution, `lw_calendar_execution` records `(action_id, occurrence_key)`. Future evaluations skip that same action/occurrence. Failed actions are not recorded and can retry during the catch-up window.

## Rotations

A schedule may contain ordered `lw_calendar_rotation` rows. The engine derives an occurrence ordinal, applies `rotation_offset`, and selects a rotation row modulo the enabled rotation count.

If an action sets `use_rotation_target=1` and the selected row has a nonzero `target_id_override`, that ID replaces the action's normal `target_id`. This is the foundation for alternating destinations/variants such as two traveling-event definitions representing opposite caravan directions.

`value` is currently loaded as generic rotation metadata but is not consumed by a calendar action target type.

## Calendar action target types

| Value | Action |
|---:|---|
| 1 | Start invasion |
| 2 | Start traveling event |
| 3 | Stop traveling event |

Starting an invasion calls the invasion runtime directly; the invasion's SQL stages determine its duration and completion. Starting a traveling event treats `AlreadyActive` as successful. Stop traveling event calls the traveling manager's stop path.

`parameter1` and `parameter2` exist in the schema/definition but are not currently consumed by the implemented calendar action types.

## Commands

```text
.lw calendar status
.lw calendar reload
```

`.lw status` also includes the calendar status report.

## Authoring guidance

Keep calendar logic generic. Prefer multiple SQL actions with offsets and rotation data rather than adding event-specific date checks to C++. Calendar rows should determine **when** and **which target**; the target subsystem should determine **what the event does once started**.
