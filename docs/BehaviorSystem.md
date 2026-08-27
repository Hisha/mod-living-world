# Behavior System — No Generic Behavior DSL

The previously proposed generic Behavior Controller/Behavior Step SQL layer is **not part of the implementation**. Do not author against hypothetical behavior-definition/behavior-step tables.

Reusable behavior has instead evolved as focused engine capabilities attached to existing SQL concepts:

- stages and stage actions for orchestration;
- route journeys and route/movement-node actions for travel-time behavior;
- `AssaultManager` for objective-area combat acquisition;
- `GroupDefeatWatcher` for defeat-driven runtime signals;
- garrison restocking through `InvasionSpawnManager`;
- `lw_creature_ability` / `CreatureAbilityManager` for logical-template combat spells;
- `TravelingEventManager` for civilian caravan/camp lifecycle;
- `CalendarManager` for deterministic world-event timing/rotation.

This keeps each mechanic explicit and testable rather than introducing a second generic scripting language on top of stages/actions. A broader behavior DSL should only be reconsidered if future content exposes repeated behavior that these focused systems cannot express cleanly.
