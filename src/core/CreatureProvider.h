#ifndef MOD_LIVING_WORLD_CREATURE_PROVIDER_H
#define MOD_LIVING_WORLD_CREATURE_PROVIDER_H

#include "IEntityProvider.h"

namespace lw
{
class CreatureProvider final : public IEntityProvider
{
public:
    [[nodiscard]] uint8 GetType() const override;

    bool Spawn(
        uint64 runtimeId,
        SpawnGroupDefinition const& group,
        SpawnMemberDefinition const& member,
        std::vector<RuntimeEntity>& spawnedEntities) override;

    bool Cleanup(RuntimeEntity const& entity) override;
};
}

#endif
