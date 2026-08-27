# Testing and GM Commands

## Development configuration

```ini
LivingWorld.Enable = 1
LivingWorld.Debug = 1
LivingWorld.Scheduler.Enable = 1
LivingWorld.Travel.StartHour = 6
LivingWorld.Travel.EndHour = 18
```

`LivingWorld.Debug` gates invasion triggering and most in-game route/camp authoring helpers.

## Administration and runtime commands

| Command | Purpose |
|---|---|
| `.lw version` | Version, scheduler state, active runtime count, loaded definition counts |
| `.lw status` | Scheduler + invasion runtime + calendar status |
| `.lw signals` | Current runtime signal state |
| `.lw start` | Resume invasion scheduler |
| `.lw stop` | Drain scheduler; existing invasion runtimes continue |
| `.lw enable <invasionId>` | Enable invasion in DB; reload required to refresh cached definitions |
| `.lw disable <invasionId>` | Disable invasion in DB; active runtime unaffected; reload required |
| `.lw reload` | Reload LW definitions when scheduler is stopped and no invasion runtime is active |
| `.lw trigger <invasionId>` | Debug-trigger an enabled invasion |
| `.lw abort` | Show emergency-abort warning |
| `.lw abort confirm` | Emergency cleanup/termination of active invasion runtimes; scheduler remains stopped |
| `.lw travel start <eventId>` | Start traveling event |
| `.lw travel stop <eventId>` | Stop traveling event and clean it up |
| `.lw travel status` | Traveling-event definitions/active status |
| `.lw travel <eventId> spawn <routeNodeId>` | Debug-materialize that event's camp at a route node |
| `.lw travel <eventId> despawn <routeNodeId>` | Remove the matching debug camp |
| `.lw calendar status` | Calendar schedules/action counts/rotation counts |
| `.lw calendar reload` | Reload calendar definitions |

## Route commands

| Command | Purpose |
|---|---|
| `.lw route path build <StartName> <EndName>` | Record a reusable segment automatically at ~5-yard spacing |
| `.lw route path status` | Active build progress |
| `.lw route path pause` / `resume` | Pause/resume automatic recording |
| `.lw route path complete` | Finalize path/segment |
| `.lw route path cancel [confirm]` | Guarded cancellation of current automatic build |
| `.lw route path show <pathId>` / `hide` | Show/remove temporary path-node markers |
| `.lw route path nearest <pathId>` | Find nearest authored node on a path |
| `.lw route node add ...` | Add logical route node using in-game authoring handler |
| `.lw route segment add ...` | Add route segment using in-game authoring handler |
| `.lw route test <segmentId> <fromNodeId>` | Test one segment using selected creature |
| `.lw route travel <fromId|name> <toId|name>` | Test graph-resolved multi-segment journey |
| `.lw route export segment <id|name>` | Export one segment + dependencies |
| `.lw route export journey <from> <to>` | Export connected journey + dependencies |
| `.lw route export network` | Export canonical `lw_exports/801_routes.sql` |
| `.lw route action add open-go <gameObjectDbGuid> [waitMs]` | Attach physical movement-node GameObject-open action to nearest route node |
| `.lw route action list` | List actions on nearest physical movement node |
| `.lw route action remove <actionId>` | Delete movement-node action |
| `.lw route network reset [confirm]` | Guarded development reset of shared route network |

The older manual `.lw route record ...` command family is still present, but the automatic 5-yard path builder is the normal authoring path for roads.

## Normal invasion edit/test loop

```text
1. .lw stop
2. wait for active invasion runtime(s) to finish
3. edit/apply SQL
4. .lw reload
5. verify .lw version / .lw status
6. .lw trigger <id>
7. observe in game and watch logs
```

Derived `lw_creature_template` definition changes are different: they are materialized before AzerothCore loads creature templates and therefore require a **worldserver restart**.

## Regression checklist

- [ ] Fresh database schema installs successfully.
- [ ] Definition counts are sane after startup/reload.
- [ ] Scheduler selection/capacity/cooldown works.
- [ ] Spawn groups resolve route-node anchors and logical LW creature templates.
- [ ] Creature abilities cast only under configured target/cooldown/range/combat conditions.
- [ ] Route journeys work forward/reverse and across multiple segments.
- [ ] Door/GameObject route actions fire and movement resumes after hold time.
- [ ] Combat interrupts strategic movement and survivors resume afterward.
- [ ] Partial casualties do not stall intermediate route progress.
- [ ] Final objective arrival emits the expected completion signal.
- [ ] Assault groups acquire intended settlement targets around the configured assault center.
- [ ] Group-defeat watch emits only under its ANY/ALL policy.
- [ ] Garrison restock waits for quiet, respects authored counts, and does not regenerate from zero.
- [ ] Successful/failing/timed-out invasion runtimes clean tracked entities/managers.
- [ ] Traveling event materializes camp, enables vendor, departs only inside travel window, routes to next stop, and rebuilds camp.
- [ ] Debug camp spawn/despawn works without creating a normal traveling runtime.
- [ ] Calendar recurrence resolves the expected local date/time.
- [ ] Calendar rotation selects the expected target.
- [ ] Calendar catch-up executes inside the grace window and does not duplicate an already recorded occurrence.

## Known clean-install test to add immediately

A fresh database should specifically validate that `lw_traveling_event_stop` contains every column selected by `TravelingEventManager` and inserted by `901_traveling_sales_wagon.sql`. The current checked-in base SQL is missing `camp_layout_id`; see `Status.md` and `DocumentationAudit.md`.
