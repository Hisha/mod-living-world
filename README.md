# Living World

Living World (LW) is an AzerothCore module for SQL-authored dynamic world activity: staged invasions, reusable route-driven movement, traveling civilian events, and calendar-driven world events. It requires no client modification and no AzerothCore core patch.

**Current development version:** `0.3.0-dev`


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

## Hunt / Prey prototype (0.4.0-dev)

The first Hunt subsystem prototype is intentionally limited to Elwynn Forest while the encounter loop is validated.

- Hunts are independently controlled by `LivingWorld.Hunts.Enable`.
- Minimum hunt level defaults to 10 and is configurable with `LivingWorld.Hunts.MinimumLevel`.
- Future completion XP scaling is already reserved as `LivingWorld.Hunts.XPMultiplier`; reward delivery is intentionally deferred.
- Active hunt state is per-character and persisted in `lw_hunt_runtime`.
- Normal creature kills in the assigned zone build randomized tracking progress.
- The prototype performs two prey ambushes. At the configured escape-health threshold (50% in the test content), the prey becomes non-attackable and despawns.
- At 100%, an authored final location is selected and announced. A native 3.3.5 `SMSG_GOSSIP_POI` map marker is sent for the selected final location. The clickable final activation object is the next implementation step.
- `.lw hunt final` currently spawns the final prey after the player travels to the announced location.
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
