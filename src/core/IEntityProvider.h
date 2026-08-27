#ifndef MOD_LIVING_WORLD_ENTITY_PROVIDER_H
#define MOD_LIVING_WORLD_ENTITY_PROVIDER_H

#include "Define.h"
#include "ObjectGuid.h"

#include <vector>

namespace lw
{
struct SpawnGroupDefinition;
struct SpawnMemberDefinition;

enum class EntityProviderType : uint8
{
    Creature = 1,
    GameObject = 2
};

struct RuntimeEntity
{
    uint8 EntityType = 0;
    uint32 MapId = 0;
    uint32 GroupId = 0;
    uint32 MemberId = 0;
    uint32 Entry = 0;
    uint32 LwTemplateId = 0;
    uint8 TacticalRole = 0;
    ObjectGuid Guid;
};

class IEntityProvider
{
public:
    virtual ~IEntityProvider() = default;

    [[nodiscard]] virtual uint8 GetType() const = 0;

    virtual bool Spawn(
        uint64 runtimeId,
        SpawnGroupDefinition const& group,
        SpawnMemberDefinition const& member,
        std::vector<RuntimeEntity>& spawnedEntities) = 0;

    virtual bool Cleanup(RuntimeEntity const& entity) = 0;
};
}

#endif
