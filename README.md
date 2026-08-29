## 0.5.8-dev Expanded Hunt prey roster and abilities

- Expanded the legendary Hunt prey roster from 8 to 16 prey.
- Added data-driven combat personalities to every prey using existing 3.3.5a creature spells.
- Added bleeds, frenzy, knockdowns, roars, thrash, silence, fear, poison, and enrage behaviors with conservative cooldowns.
- No client patch and no C++ Hunt behavior changes are required for the new prey content.

## 0.5.8-dev Shared final-prey kill credit

- Final prey completion is now resolved from the Hunt runtime that spawned the creature, not only from the player who landed the killing blow.
- The owning hunter receives credit when they get the kill, when a member of their current group gets the kill, or when normal creature tap rules show that the hunter/group tagged the prey.
- Nearby group members (within 200 yards) who are in the same final-stage prey/zone hunt also receive shared completion credit.
- Shared completion clears duplicate final prey/activators, advances credited hunters to `ReadyToTurnIn`, persists immediately, and stops the final-location POI refresh.
- Unrelated outsiders may still help, but do not receive Hunt credit; an uncredited outsider kill leaves the owner's hunt recoverable so the crystal can return.
- Final prey kills no longer fall through into ordinary tracking progress.

## 0.5.6-dev Hunt recovery persistence fix

- Corrects the Hunt prey loader to use the canonical `lw_hunt_prey` table.
- Corrects active-hunt persistence to use the canonical `prey_id` runtime column.
- Loads and executes `lw_hunt_prey_ability` rows during both ambush and final encounters.
- Preserves the non-grey/XP-eligible kill requirement for ordinary tracking progress.
- Expands `910_beast_prey.sql` to eight prey: Ashfang, Silkmaw, Gorehide, Whiteclaw, Tidefang, Stonegut, Sootfang, and Nightfang.
- Keeps Silkmaw's Web/Poison and Gorehide's Charge as the first data-driven prey abilities.

## 0.5.3-dev Hunt prey expansion and tracking eligibility

- Fixes first-completion Hunt statistics persistence by writing `lw_hunt_stats` synchronously before confirming turn-in.
- Adds level-banded prey health scaling. The existing hunt multipliers remain the level-70+ ceiling; lower levels scale down automatically (10-19: 1.5x ambush / 3.0x final with the default 4x/6x hunt values).
- Keeps prey visually Elite, but hunt health scaling now owns the health pool instead of allowing the cloned elite template health to override it.
- Adds `LivingWorld.Hunts.SearchScope`: `0=Local Region`, `1=Continent`, `2=World` (configured default in this package is `0=Local Region`).
- Adds data-driven, overlapping Huntmaster local-region mappings in `909_hunt_local_regions.sql`.
- Adds logical continent IDs so BC-map starting zones and Quel'Danas are grouped with their actual Azeroth continent rather than map 530/Outland.

# Living World

Living World (LW) is an AzerothCore module for SQL-authored dynamic world activity: staged invasions, reusable route-driven movement, traveling civilian events, and calendar-driven world events. It requires no client modification and no AzerothCore core patch.

### 0.5.3 Hunt changes

- Ordinary kills only advance Hunt tracking when the creature is non-grey under AzerothCore/WotLK XP color rules.
- Hunt prey kills remain exempt from the ordinary tracking eligibility check.
- Adds the non-grey/XP-eligibility tracking rule; prey content is maintained in `910_beast_prey.sql`.

**Current development version:** `0.5.8-dev`


## Repository layout

```text
mod-living-world/
├── conf/                         # LivingWorld.* configuration
├── data/sql/
│   ├── db-characters/base/       # runtime persistence schema
│   └── db-world/
│       ├── base/                 # canonical framework schema
│       └── prebuilt/             # optional authored content shipped by this repo
├── docs/
└── src/
    ├── core/                     # shared routes, movement, entities, actions, data services
    ├── invasions/                # invasion scheduler/runtime/spawning
    ├── travelers/                # persistent traveling-world-event runtime
    ├── LivingWorldScripts.cpp    # AzerothCore hooks and .lw command surface
    └── mod_living_world_loader.cpp
```

The core is intentionally reusable. A gameplay subsystem may depend on core services, but shared core code should not depend on invasion- or traveler-specific runtime behavior. Future systems such as hunts can therefore be added beside `invasions/` and `travelers/` instead of being implemented as special cases of either one.

## Clean break from mod-living-world-invasions

This repository is the successor to the development-only `mod-living-world-invasions` prototype. There is intentionally no compatibility/migration layer. For the current development server, remove the old module directory and drop the old prototype tables whose names begin with `lwi_` before installing this module. The canonical `lw_*` schema and shipped prebuilt SQL recreate the authored route network, Westfall invasion, and traveling salesman content from the repository.

Do not keep both module directories under AzerothCore `modules/` at the same time.

## Current capabilities

The current codebase includes four major layers:

- **Invasion runtime** — weighted scheduling, cooldowns/capacity, ordered stages, runtime signals, spawn groups, assault/garrison behavior, defeat watches, dialogue, announcements, sounds, spells, cleanup, and runtime persistence.
- **Shared route network** — stable route nodes, bidirectional segments, graph travel, semantic node actions, 5-yard in-game path authoring, gate/GameObject route actions, testing, and SQL export.
- **LW creature layer** — portable logical creature templates with dynamically allocated AzerothCore entries plus data-driven combat abilities.
- **World-event layer** — non-invasion traveling events with route-driven merchant/mule caravans, reusable camp layouts, daily travel windows, and a generic server-local calendar/rotation engine that can start invasions or start/stop traveling events.

The shipped prebuilt content now includes the **Defias Westfall invasion** (`900_defias_westfall_invasion.sql`) and the **Traveling Salesman** (`901_traveling_sales_wagon.sql`). These are active development content, not the old scheduler-only test scenario.

## Quick start

1. Build/install the module normally with AzerothCore.
2. Copy `conf/mod_living_world.conf.dist` to the module configuration location used by your installation.
3. Start worldserver and verify the `lw_*` tables install and definitions load.
4. Enable `LivingWorld.Debug = 1` while authoring/testing content.
5. Use `.lw version`, `.lw status`, `.lw travel status`, and `.lw calendar status` to inspect the framework.
6. Follow [docs/CreatingAnInvasion.md](docs/CreatingAnInvasion.md) for invasion content and [docs/TravelingWorldEvents.md](docs/TravelingWorldEvents.md) for traveling events.

## Documentation

- [Clean-break Install](docs/CleanBreakInstall.md) — replacing the development-only invasion module on an existing server.
- [Architecture](docs/Architecture.md) — current subsystem ownership and runtime flow.
- [Database Schema](docs/DatabaseSchema.md) — current world/runtime tables and action mappings.
- [Creating an Invasion](docs/CreatingAnInvasion.md) — SQL-first invasion authoring workflow.
- [Shared Route Network](docs/RouteNetwork.md) — route graph, authoring, testing, GameObject actions, and export.
- [Traveling World Events](docs/TravelingWorldEvents.md) — merchant/mule caravan and camp lifecycle.
- [Calendar and Rotation](docs/Calendar.md) — dated/recurring schedules, offsets, catch-up, and alternating targets.
- [Testing](docs/Testing.md) — GM commands and validation checklist.
- [Current Status](docs/Status.md) — implemented systems, proven content, limitations, and known issues.
- [Roadmap](docs/Roadmap.md) — next framework/content work.
- [Documentation Audit](docs/DocumentationAudit.md) — documentation refresh findings against this codebase.
- [Coding Standards](docs/CodingStandards.md) — project conventions.

## Important development rules

The files under `data/sql/*/base` are the canonical clean-install schema for this pre-release module. AzerothCore's database updater tracks their hashes. During development, do not manually import those same repository update files and then also expect the updater to manage them unchanged. Prebuilt content intended to be reapplied should be idempotent.

LW is a framework rather than a collection of hard-coded events. Event-specific tuning should live in SQL wherever the framework supports it; C++ should provide reusable mechanics.

### Route network publishing

Use `.lw route export network` with `LivingWorld.Debug = 1` to generate `lw_exports/801_routes.sql`, then copy it into `data/sql/db-world/prebuilt/801_routes.sql` when publishing the canonical route network. Published route-node IDs are data contracts and should not be renumbered casually.

## Hunt / Prey prototype (0.4.2-dev)

The first Hunt subsystem prototype is intentionally limited to Elwynn Forest while the encounter loop is validated.

- Hunts are independently controlled by `LivingWorld.Hunts.Enable`.
- Minimum hunt level defaults to 10 and is configurable with `LivingWorld.Hunts.MinimumLevel`.
- Future completion XP scaling is already reserved as `LivingWorld.Hunts.XPMultiplier`; reward delivery is intentionally deferred.
- Active hunt state is per-character and persisted in `lw_hunt_runtime`.
- Normal creature kills in the assigned zone build randomized tracking progress.
- The prototype performs two prey ambushes. At the configured escape-health threshold (50% in the test content), the prey becomes non-attackable and despawns.
- At 100%, an authored final location is selected and announced. A native 3.3.5 `SMSG_GOSSIP_POI` map marker is sent for the selected final location.
- When the hunter approaches the marked site, Living World spawns a clickable prey-trail marker using an existing 3.3.5 client GameObject visual. Clicking it begins the final encounter; `.lw hunt final` remains available only as a GM/debug fallback.
- Prototype prey can reference an LW logical creature template, allowing a custom name/rank/combat shell while reusing an existing client model. Ashfang now uses this path.
- Hunt prey health is floored relative to the hunter's own maximum health so high-level characters cannot trivially one-shot a low-level visual base.
- Killing the final prey changes the contract to ready-to-turn-in; the player must return to the exact Huntmaster spawn that issued it.

### First test setup

`902_elwynn_hunt_test.sql` creates Huntmaster Corvin (entry `14999980`) but deliberately does not permanently place him. After startup, stand where you want the temporary Stormwind Huntmaster and run:

```text
.npc add 14999980
```

Talk to **Huntmaster Corvin <Master of the Hunt>** and choose **I seek dangerous prey.**

Useful GM/debug commands:

```text
.lw hunt status
.lw hunt progress <amount>
.lw hunt ambush
.lw hunt final
.lw hunt abandon
```


## Hunt system (0.5.8-dev)

The Hunt world rollout now includes 10 permanent Huntmasters (the eight racial capitals plus neutral Shattrath and Dalaran) and three curated final encounter sites for every enabled Eastern Kingdoms hunt zone. Final-site sub-area names are resolved from AreaTable.dbc at runtime when no explicit location name is authored.

Hunts are an optional Living World subsystem with permanent Huntmasters in the eight faction capitals. Capital guards can point players to their local Huntmaster without replacing the stock guard direction menus. Hunt target selection and hunt-zone selection are independent and level-aware; hunt zones have no faction restriction. Final encounter sites are authored separately per zone so crystals can be kept away from hostile settlements. Only zones with at least one enabled final site are eligible for assignment. Huntmasters also expose a persistent per-character hunting record through gossip.


### 0.5.6 recovery persistence fix

- `tracking_progress >= 100` is now the recovery source of truth, so both `state=1/final_location_id=0` and `state=2/final_location_id=0` self-heal.
- Hunt runtime saves now use synchronous character-database writes for state transitions, preventing the database from lagging behind the in-memory hunt state during logout/restart.
- If a valid final location was saved but the state was not, the runtime promotes to `FinalLocated` without rerolling the authored site.

### 0.5.5 final-location recovery

Hunts now refuse to transition into the final-located state without a valid authored final location. Existing runtimes found in `FinalLocated` with a missing/invalid `final_location_id` are automatically repaired. The native guard-style POI for final prey is resent every five seconds while the hunt remains in the final-located state, including after the crystal has been activated, and stops only after the prey kill is credited.