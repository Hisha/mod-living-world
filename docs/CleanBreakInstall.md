# Clean-break install from mod-living-world-invasions

`mod-living-world` intentionally does not provide a compatibility layer for the development-only `mod-living-world-invasions` prototype.

## Before building

1. Keep the old Git repository as a reference/rollback point.
2. Remove the old `mod-living-world-invasions` directory from AzerothCore `modules/`.
3. Clone `mod-living-world` into AzerothCore `modules/`.
4. Remove the old prototype database tables beginning with `lwi_` from both the world and characters databases. Do not drop normal AzerothCore tables.

The new module uses only the `lw_*` prefix. Its base SQL recreates the framework schema and its shipped prebuilt SQL restores the authored shared routes, Defias Westfall invasion, and traveling salesman content.

## Build/install

From the AzerothCore build directory, rerun CMake so the module list and generated script loader are refreshed, then build and install normally:

```bash
cmake ..
make -j"$(nproc)"
sudo make install
sudo systemctl restart azeroth-world
```

Use the same CMake options and service name used by your existing AzerothCore installation if they differ from the example above.

## First-start checks

After worldserver starts:

```text
.lw version
.lw status
.lw travel status
.lw calendar status
```

Confirm that the module reports version `0.3.0-dev`, that `lw_*` definitions load without SQL errors, and that no old module is also registered.

## Configuration

The master switch is `LivingWorld.Enable`. Current gameplay subsystems are independently switchable:

```ini
LivingWorld.Invasions.Enable = 1
LivingWorld.Travelers.Enable = 1
```

Shared route/data services remain part of Living World core.
