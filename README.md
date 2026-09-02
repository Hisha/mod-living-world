# Living World

Living World (LW) is an AzerothCore module for SQL-authored dynamic world activity: staged invasions, reusable route-driven movement, traveling civilian events, and calendar-driven world events. It requires no client modification and no AzerothCore core patch.

**Current development version:** `0.7.1-dev`

> **Hunts have moved.** The Hunt/Prey system, including Huntmasters, tracking, normal and Elite Hunts, final locations, rewards, statistics, and Hunt authoring tools, is now maintained as the completely standalone `mod-hunts` module. `mod-living-world` has no build, runtime, configuration, command, or database dependency on Hunts.

## Repository layout

```text
mod-living-world/
├── conf/                         # LivingWorld.* configuration
├── data/sql/
│   ├── db-characters/base/       # Living World runtime persistence
│   └── db-world/
│       ├── base/                 # canonical framework schema
│       └── prebuilt/             # authored content shipped by this repo
├── docs/
└── src/
    ├── core/                     # routes, movement, entities, actions, data services
    ├── invasions/                # invasion scheduler/runtime/spawning
    ├── travelers/                # persistent traveling-world-event runtime
    ├── LivingWorldScripts.cpp    # AzerothCore hooks and .lw command surface
    └── mod_living_world_loader.cpp
```

The core is intentionally reusable. Gameplay subsystems may consume shared core services, while the core remains independent of invasion- or traveler-specific behavior.

## Current capabilities

Living World currently provides four major layers:

- **Invasion runtime** — weighted scheduling, cooldowns/capacity, ordered stages, runtime signals, spawn groups, assault/garrison behavior, defeat watches, dialogue, announcements, sounds, spells, cleanup, and runtime persistence.
- **Shared route network** — stable route nodes, bidirectional segments, graph travel, semantic node actions, 5-yard in-game path authoring, gate/GameObject route actions, testing, and SQL export.
- **LW creature layer** — portable logical creature templates with dynamically allocated AzerothCore entries plus data-driven combat abilities. This framework remains local to Living World and is independent of the equivalent machinery used by `mod-hunts`.
- **World-event layer** — non-invasion traveling events with route-driven merchant/mule caravans, reusable camp layouts, daily travel windows, and a generic server-local calendar/rotation engine that can start invasions or start/stop traveling events.

The shipped prebuilt content includes the **Defias Westfall invasion** (`900_defias_westfall_invasion.sql`) and the **Traveling Salesman** (`901_traveling_sales_wagon.sql`).

## Hunt subsystem extraction

Beginning with `0.7.1-dev`, Hunt gameplay is no longer part of Living World. The extraction intentionally creates two independent modules:

```text
mod-living-world                 mod-hunts
├── invasions                    ├── Huntmasters
├── routes                       ├── normal Hunts
├── movement                     ├── Elite Hunts
├── travelers                    ├── prey combat AI
├── calendar/world events        ├── tracking/final locations
└── LW creature framework        └── rewards/statistics/authoring
```

Living World no longer owns or loads Hunt runtime code, `LivingWorld.Hunts.*` configuration, `.lw hunt ...` commands, or `lw_hunt_*` tables. Hunt development continues in `mod-hunts` using its own `Hunts.*` configuration, `.hunt ...` commands, `hunt_*` schema, and Hunt-owned dynamic creature-template implementation.

The modules may be installed together or independently; neither requires the other.

## Clean break from mod-living-world-invasions

This repository is the successor to the development-only `mod-living-world-invasions` prototype. There is intentionally no compatibility/migration layer for that old prototype. Remove the old module directory and drop obsolete prototype tables whose names begin with `lwi_` before installing Living World.

The canonical `lw_*` schema and shipped prebuilt SQL recreate the authored route network, Westfall invasion, and Traveling Salesman content from the repository. Do not keep both `mod-living-world-invasions` and `mod-living-world` under AzerothCore `modules/` at the same time.

## Quick start

1. Build/install the module normally with AzerothCore.
2. Copy `conf/mod_living_world.conf.dist` to the module configuration location used by your installation.
3. Start worldserver and verify the `lw_*` tables install and Living World definitions load.
4. Enable `LivingWorld.Debug = 1` while authoring/testing content.
5. Use `.lw version`, `.lw status`, `.lw travel status`, and `.lw calendar status` to inspect the framework.
6. Follow `docs/CreatingAnInvasion.md` for invasion content and `docs/TravelingWorldEvents.md` for traveling events.

## Documentation

- `docs/CleanBreakInstall.md` — replacing the development-only invasion module on an existing server.
- `docs/Architecture.md` — subsystem ownership and runtime flow.
- `docs/DatabaseSchema.md` — world/runtime tables and action mappings.
- `docs/CreatingAnInvasion.md` — SQL-first invasion authoring workflow.
- `docs/RouteNetwork.md` — route graph, authoring, testing, GameObject actions, and export.
- `docs/TravelingWorldEvents.md` — merchant/mule caravan and camp lifecycle.
- `docs/Calendar.md` — dated/recurring schedules, offsets, catch-up, and alternating targets.
- `docs/Testing.md` — GM commands and validation checklist.
- `docs/Status.md` — implemented systems, proven content, limitations, and known issues.
- `docs/Roadmap.md` — next framework/content work.
- `docs/CodingStandards.md` — project conventions.

## Development rules

The files under `data/sql/*/base` are the canonical clean-install schema for this pre-release module. AzerothCore's database updater tracks their hashes. During development, do not manually import those same repository update files and then also expect the updater to manage them unchanged. Prebuilt content intended to be reapplied should be idempotent.

Living World is a framework rather than a collection of hard-coded events. Event-specific tuning should live in SQL wherever the framework supports it; C++ should provide reusable mechanics.

### Route network publishing

Use `.lw route export network` with `LivingWorld.Debug = 1` to generate `lw_exports/801_routes.sql`, then copy it into `data/sql/db-world/prebuilt/801_routes.sql` when publishing the canonical route network. Published route-node IDs are data contracts and should not be renumbered casually.

## Status

The Hunt extraction is complete as of `0.7.1-dev`. Living World and standalone Hunts can initialize side-by-side while maintaining separate schemas, runtime state, configuration, commands, and dynamic creature-template mappings.