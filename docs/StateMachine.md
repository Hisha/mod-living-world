# State Machines

## Invasion scheduler state

Persisted per invasion in `acore_characters.lw_invasion_runtime`:

```text
Available -> Active -> Cooldown -> Available
```

Successful all-stage completion increments completion state/counters and enters cooldown. Framework failure or hard timeout also enters cooldown but is not counted as successful completion. A start failure returns the scheduler record to availability.

## Scheduler control state

The invasion scheduler can be Running, Paused, or Draining. `.lw stop` drains: no new random invasions start while existing invasion runtimes continue.

## Invasion runtime

```text
Stage 1 -> Stage 2 -> ... -> final Stage -> Completed
                         \\-> Failed / Timeout / Abort
```

Each stage waits for Timer or Runtime Signal completion. Specialized managers (movement, assault, defeat watch, garrison, abilities) are subordinate to the runtime and are cancelled/cleaned when the runtime ends.

## Traveling-event state

```text
Camped -> Traveling -> Camped -> Traveling -> ...
```

A camped event dwells at its current stop. When dwell expires, departure waits until the configured daily travel window is open. Travel uses the shared route graph to the next ordered stop; arrival materializes the camp and restarts dwell.

## Calendar execution state

The calendar is occurrence/action based rather than a long-lived runtime state machine:

```text
Occurrence becomes due
 -> outside catch-up window: skip
 -> already recorded: skip
 -> execute target action
      -> success: record action + occurrence key
      -> failure: leave unrecorded and retry while still inside catch-up window
```
