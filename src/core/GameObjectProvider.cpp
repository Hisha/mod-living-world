#include "GameObjectProvider.h"

#include "GameObject.h"
#include "LivingWorld.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "Position.h"
#include "Random.h"

#include <cmath>

namespace lw
{
namespace
{
bool ResolveSpawnAnchor(SpawnGroupDefinition const& group, uint16& mapId, float& x, float& y, float& z, float& orientation)
{
    if (group.UseExplicitPosition)
    {
        mapId = group.MapId;
        x = group.X;
        y = group.Y;
        z = group.Z;
        orientation = group.Orientation;
        return true;
    }

    RouteNodeDefinition const* routeNode = sLivingWorldDataMgr.GetRouteNode(group.RouteNodeId);
    if (!routeNode)
        return false;

    mapId = routeNode->MapId;
    x = routeNode->X;
    y = routeNode->Y;
    z = routeNode->Z;
    orientation = routeNode->Orientation;
    return true;
}

Position BuildSpawnPosition(SpawnGroupDefinition const& group, float x, float y, float z, float orientation)
{
    Position position;
    position.Relocate(x, y, z, orientation);

    if (group.SpawnRadius > 0.0f)
    {
        constexpr float TwoPi = 6.28318530717958647692f;
        float angle = frand(0.0f, TwoPi);
        float distance = frand(0.0f, group.SpawnRadius);

        position.m_positionX += std::cos(angle) * distance;
        position.m_positionY += std::sin(angle) * distance;
    }

    return position;
}
}

uint8 GameObjectProvider::GetType() const
{
    return static_cast<uint8>(EntityProviderType::GameObject);
}

bool GameObjectProvider::Spawn(
    uint64 runtimeId,
    SpawnGroupDefinition const& group,
    SpawnMemberDefinition const& member,
    std::vector<RuntimeEntity>& spawnedEntities)
{
    uint16 mapId = 0;
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    float anchorZ = 0.0f;
    float anchorOrientation = 0.0f;
    if (!ResolveSpawnAnchor(group, mapId, anchorX, anchorY, anchorZ, anchorOrientation))
    {
        LOG_ERROR("server.loading",
            "[LW GameObject] Spawn group {} references unavailable route node {}.",
            group.Id, group.RouteNodeId);
        return false;
    }

    Map* map = sMapMgr->FindMap(mapId, 0);
    if (!map)
    {
        LOG_ERROR("server.loading",
            "[LW GameObject] Could not find map {} for spawn group {} route node {}.",
            mapId, group.Id, group.RouteNodeId);
        return false;
    }

    bool spawnedAny = false;

    for (uint32 i = 0; i < member.Count; ++i)
    {
        Position position = BuildSpawnPosition(group, anchorX, anchorY, anchorZ, anchorOrientation);

        GameObject* gameObject = map->SummonGameObject(
            member.EntityEntry,
            position,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            0,
            true);

        if (!gameObject)
        {
            LOG_ERROR("server.loading",
                "[LW GameObject] Runtime #{} failed to spawn gameobject entry {} from member {}.",
                runtimeId,
                member.EntityEntry,
                member.Id);
            continue;
        }

        RuntimeEntity entity;
        entity.EntityType = GetType();
        entity.MapId = mapId;
        entity.GroupId = group.Id;
        entity.MemberId = member.Id;
        entity.Entry = member.EntityEntry;
        entity.Guid = gameObject->GetGUID();
        spawnedEntities.push_back(std::move(entity));

        LOG_INFO("server.loading",
            "[LW GameObject] Runtime #{} spawned gameobject {} from member {} GUID {}.",
            runtimeId,
            member.EntityEntry,
            member.Id,
            gameObject->GetGUID().ToString());

        spawnedAny = true;
    }

    return spawnedAny;
}

bool GameObjectProvider::Cleanup(RuntimeEntity const& entity)
{
    Map* map = sMapMgr->FindMap(entity.MapId, 0);
    if (!map)
    {
        return false;
    }

    GameObject* gameObject = map->GetGameObject(entity.Guid);
    if (!gameObject)
    {
        return false;
    }

    LOG_INFO("server.loading",
        "[LW GameObject] Deleting gameobject entry {} GUID {}.",
        entity.Entry,
        entity.Guid.ToString());

    gameObject->Delete();

    return true;
}
}
