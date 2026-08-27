# Shared Route Network

LW includes a reusable world-travel graph that is intentionally independent of any one invasion. A road, trail, gate connection, or other travel corridor can be authored once and reused later by invasions, response forces, patrols, merchants, caravans, civilian events, and other Living World systems.

## Data model

The shared route network has two graph-level tables:

- `lw_route_node` — a logical connection point such as a settlement, road junction, gate, bridge, dock, or trail intersection;
- `lw_route_segment` — a bidirectional connection between two route nodes backed by one `lw_movement_path`.

The physical coordinates remain in the existing movement tables:

- `lw_movement_path` — names the physical path;
- `lw_movement_node` — ordered coordinates followed along that path.

A segment does not belong to an invasion. Consumers choose the direction at runtime. If a segment is defined as `A -> B`, the same movement-node data can be traversed `A -> B` or `B -> A`.

## Graph authoring rule

Prefer route nodes at places where travel decisions can change:

- road intersections and forks;
- settlements and camps;
- gates and bridges;
- docks or future transport transition points;
- other meaningful connection points.

Do not create one giant town-to-town segment when the road passes a junction that another route may need. Junction-to-junction segments maximize reuse and allow graph routing to select the correct branch automatically.

## Recommended automatic builder

The preferred authoring workflow is the automatic path builder. It records the GM's actual traveled path every **5 yards**. The deliberately dense spacing avoids long point-to-point chords that can cut outside road corridors, fences, curves, bridges, or other world geometry.

Stand at the beginning of the desired segment and run:

```text
.lw route path build <StartName> <EndName>
```

Example:

```text
.lw route path build Stormwind_Gate Goldshire
```

Then walk or ride the exact route you want creatures to use. Coordinates are written to the world database continuously while you travel. At the endpoint run:

```text
.lw route path complete
```

The builder:

1. reuses an existing named start/end route node when one exists;
2. creates missing logical route nodes;
3. allocates the movement-path and route-segment IDs;
4. records the exact starting position;
5. writes movement nodes every 5 yards of traveled distance;
6. records/snaps the exact final endpoint;
7. creates the reusable `lw_route_segment` automatically.

The builder currently records a continuous segment on one AzerothCore map. Crossing a **zone** boundary is fine as long as the map ID does not change. True map transitions such as Eastern Kingdoms to Kalimdor will require a future transport/transition edge rather than one continuous movement segment.

### Builder commands

```text
.lw route path build <StartName> <EndName>
.lw route path status
.lw route path pause
.lw route path resume
.lw route path complete
.lw route path cancel
.lw route path cancel confirm
```

`cancel confirm` deletes the unfinished movement path/nodes and removes a start route node if that node was created specifically by the canceled build.

## Inspecting a recorded path

With `LivingWorld.Debug = 1`:

```text
.lw route path show <pathId>
.lw route path nearest <pathId>
.lw route path hide
```

`show` places temporary visual waypoint markers at the loaded movement nodes. `nearest` reports the closest authored movement node to the GM.

## Testing a segment

After authoring or editing route data:

```text
.lw stop
.lw reload
```

Select a disposable creature and test a single segment from either endpoint. Segment and node arguments may be numeric IDs or exact names:

```text
.lw route test <segmentId|name> <fromNodeId|name>
```

Examples:

```text
.lw route test 1060 10
.lw route test Stormwind_Gate_Goldshire Stormwind_Gate
```

The same segment should be tested both forward and reverse.

## Testing graph travel

Once multiple segments are connected, select a disposable creature and run either numeric IDs or exact route-node names:

```text
.lw route travel <fromNodeId|name> <destinationNodeId|name>
```

Examples:

```text
.lw route travel 10 70
.lw route travel Stormwind_Gate Sentinel_Hill_Tower
```

LW resolves the shortest connected route by segment count and automatically chains the necessary route segments. Branches that do not lead to the requested destination are ignored.

## Manual authoring tools

The older/manual tools remain available for special cases:

```text
.lw route record start <pathId> <pathName>
.lw route record add
.lw route record undo
.lw route record status
.lw route record finish
.lw route record cancel
.lw route record cancel confirm

.lw route node add <nodeId> <nodeName>
.lw route segment add <segmentId> <segmentName> <startNodeId> <endNodeId> <pathId>
```

For normal roads, prefer the automatic 5-yard builder so waypoint density is deterministic instead of manually estimated.

## Resetting development route data

During development only, the guarded command below removes the shared route graph and the movement paths referenced by its segments while preserving movement paths that are not owned by `lw_route_segment`:

```text
.lw route network reset
.lw route network reset confirm
```

Run `.lw reload` afterward.

## Exporting portable route SQL

The in-game builder writes directly to the world database while authoring. Finished routes can be exported into self-contained SQL suitable for shared route packs or invasion-specific prebuilt SQL. Export commands require `LivingWorld.Debug = 1`.

Export one segment by numeric ID or exact segment name:

```text
.lw route export segment <segmentId|name>
```

Export the connected graph journey between two route nodes, again using IDs or names:

```text
.lw route export journey <fromNodeId|name> <destinationNodeId|name>
```

Export the complete currently loaded route network as the canonical prebuilt route file:

```text
.lw route export network
```

This writes `lw_exports/801_routes.sql`. The intended publish workflow is to copy that generated file to `data/sql/db-world/prebuilt/801_routes.sql`. Once other prebuilt invasion SQL references route-node IDs, those published route-node IDs should be treated as stable data contracts. Add new IDs freely, but do not renumber already-published route nodes without also updating every consumer.

Examples:

```text
.lw route export segment Stormwind_Gate_Goldshire
.lw route export journey Stormwind_Gate Sentinel_Hill_Tower
.lw route export network
```

Exports are written to an `lw_exports` directory beneath the worldserver working directory. The command prints the absolute output filename in-game. Exported SQL contains the required `lw_route_node`, `lw_movement_path`, `lw_movement_node`, movement-node action, and `lw_route_segment` data in dependency-safe order. Journey exports de-duplicate route nodes and movement paths used by multiple segments.

The canonical `801_routes.sql` should own the route network itself, including invasion-specific route segments whose endpoints are referenced by prebuilt invasion data. Invasion SQL should reference stable route-node IDs rather than duplicate movement-node data. Segment/journey export remains useful for review, debugging, or extracting a smaller subset, while `export network` is the normal publish path for the module's complete route dataset.


## Invasion integration

`801_routes.sql` is the canonical spatial/route data contract consumed by prebuilt invasions. Published route-node IDs are stable.

Prebuilt invasion SQL should reference route nodes only:

```text
spawn group -> route_node_id
movement action -> start_route_node_id + destination_route_node_id
route event -> route_node_id
```

Do not duplicate route-owned movement paths or dense 5-yard movement nodes inside invasion SQL. Invasion-specific physical routes are still valid route-network content and should be exported into `801_routes.sql`; the invasion file simply references their stable endpoints.

A semantic event anchor does not have to be a major road junction. Create a route node anywhere an invasion needs a stable location for a say/yell/announcement/sound. Route-node actions are scoped by invasion and spawn group, so another consumer traversing the same road does not inherit that event.

## Physical route actions for doors and gates

Some route problems belong to the dense physical path rather than to a semantic invasion route node. `lw_movement_node_action` supports physical-node actions, and the current authoring helper can attach an `OPEN_GAMEOBJECT` action (type `4`) to the nearest loaded movement node.

Stand near the recorded route node where the door/gate should be opened and use the **GameObject database GUID**, not the template entry:

```text
.lw route action add open-go <gameObjectDbGuid> [waitMs]
.lw route action list
.lw route action remove <actionId>
```

The default movement hold is 2000 ms. Run `.lw reload` before testing after adding/removing an action. Because the action is attached to the physical movement node, it is encountered when that node is traversed in either direction and is included by route SQL export.
