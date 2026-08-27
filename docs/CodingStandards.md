# Coding and Content Standards

## Primary rule

Keep invasion-specific content in SQL. Add C++ only for reusable framework mechanics.

## C++ conventions

- Follow AzerothCore/module formatting and existing `.clang-format`.
- Keep managers/providers single-purpose.
- Do not store permanent definition data in runtime objects when an ID/reference is sufficient.
- Runtime GUIDs/state must not be written into `acore_world` definition tables.
- Prefer explicit log messages prefixed with the owning subsystem, for example `[LW Movement]`.
- Failure paths must clean transient movement/signals/entities and leave scheduler state coherent.

## SQL conventions

- Use explicit column lists for INSERTs.
- Give every definition a stable numeric ID and descriptive name/comment.
- Keep related IDs in a recognizable range.
- Use `stage_order`, `action_order`, and `node_order` in increments that leave room for inserts (10, 20, 30 is preferred for authored content).
- Development/test SQL that AzerothCore may reapply must be idempotent for the IDs it owns.
- Delete dependent rows before parent rows when resetting test data.
- Never use broad deletes that could remove another invasion's shared definitions.

## Documentation rule

When a framework behavior changes, update at least:

- `docs/Status.md`;
- `docs/DatabaseSchema.md` if schema/value semantics changed;
- `docs/CreatingAnInvasion.md` if author workflow changed;
- `docs/Testing.md` if validation/log/command behavior changed;
- `docs/Roadmap.md` when a capability moves between planned and implemented.