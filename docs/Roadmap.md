# Living World Roadmap

## Completed framework/content milestones

- [x] SQL-driven invasion/stage/action definitions
- [x] Scheduler, cooldowns, selection weights, and capacity limits
- [x] Runtime persistence and active runtime manager
- [x] Timer and runtime-signal stage completion
- [x] Creature/GameObject runtime providers and cleanup
- [x] Logical LW-derived creature templates
- [x] Data-driven LW creature combat abilities
- [x] Tactical roles and role-aware formation movement
- [x] Reusable route-node/segment graph and graph journey resolution
- [x] Automatic 5-yard in-game route recorder
- [x] Route-node semantic actions
- [x] Route GameObject/door opening actions
- [x] Route SQL export for segment, journey, and complete network
- [x] Combat interruption/resume and casualty-tolerant movement
- [x] Assault behavior
- [x] Group-defeat signal watches
- [x] Garrison restocking
- [x] Defias Westfall invasion prebuilt content
- [x] Traveling merchant/caravan runtime
- [x] Reusable traveling-event camp layouts and terrain Z overrides
- [x] Daily server-local traveling window
- [x] Traveling Salesman prebuilt content
- [x] Generic calendar/rotation engine

## Immediate maintenance

- [ ] Fix clean-install traveling schema so `lw_traveling_event_stop.camp_layout_id` exists in canonical base SQL.
- [ ] Update prototype-era traveling schema comments/column naming to match the current merchant + pack-mule implementation, ideally with a clean pre-release schema consolidation rather than another compatibility layer.
- [ ] Add regression coverage for a completely fresh database install, not only an evolved development database.
- [ ] Add calendar SQL examples/prebuilt content once the first real scheduled event is ready.

## Darkmoon / scheduled-world-event direction

The calendar engine now provides the timing/rotation foundation needed for seasonal or recurring content without hard-coding dates in C++.

- [x] Generic recurrence rules
- [x] Per-action day/minute offsets
- [x] Alternating rotation targets
- [x] Catch-up and duplicate-execution protection
- [ ] Define Darkmoon site/setup/open/close content model
- [ ] Build the Darkmoon caravan traveling-event definitions/routes
- [ ] Connect calendar phases to the required world-state/materialization actions
- [ ] Validate alternating location alignment against the intended historical schedule

## Framework capabilities to revisit when content requires them

### Stage/action system

- [ ] Objective completion type
- [ ] Manual completion type
- [ ] Action delay scheduling
- [ ] Additional dialogue/emote actions
- [ ] Additional scripted spell target modes

### Movement/routes

- [ ] Transport/transition edges for true map changes (boats, zeppelins, portals)
- [ ] Apply movement-profile speed multipliers
- [ ] Apply movement-profile stealth
- [ ] Patrol/loop route helper
- [ ] Escort/follow helper
- [ ] Destination-only movement helper

### Gameplay/player interaction

- [ ] Generalized kill/area/GameObject/escort objectives
- [ ] Player participation/contribution tracking
- [ ] Rewards and credit
- [ ] Achievements

### Playerbots

- [ ] Invasion awareness and alert propagation
- [ ] Response recruitment and party/raid formation
- [ ] Participation without making Playerbots a hard dependency

### Reliability/administration

- [ ] Complete world-entity recovery after worldserver restart
- [ ] Orphan runtime-entity detection
- [ ] Stronger cross-reference validation
- [ ] Broken-route diagnostics
- [ ] Additional runtime inspection commands

## Future content

- [ ] Darkmoon Faire/caravan
- [ ] Duskwood invasion
- [ ] Wetlands invasion
- [ ] Horde-zone invasions
- [ ] Additional merchants, patrols, convoys, and civilian events
- [ ] Seasonal events and world bosses
- [ ] Multi-zone and cross-continent events
