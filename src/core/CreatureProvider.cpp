#include "CreatureProvider.h"

#include "Creature.h"
#include "LivingWorld.h"
#include "LwCreatureTemplateManager.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "Position.h"
#include "Random.h"
#include "TemporarySummon.h"

#include <algorithm>
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

char const* TacticalRoleName(TacticalRole role)
{
    switch (role)
    {
        case TacticalRole::Default:
            return "Default";
        case TacticalRole::Commander:
            return "Commander";
        case TacticalRole::Protector:
            return "Protector";
        case TacticalRole::MeleeDps:
            return "MeleeDps";
        case TacticalRole::RangedDps:
            return "RangedDps";
        case TacticalRole::Healer:
            return "Healer";
        case TacticalRole::Support:
            return "Support";
        default:
            return "Unknown";
    }
}
}

uint8 CreatureProvider::GetType() const
{
    return static_cast<uint8>(EntityProviderType::Creature);
}

bool CreatureProvider::Spawn(
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
            "[LW Creature] Spawn group {} references unavailable route node {}.",
            group.Id, group.RouteNodeId);
        return false;
    }

    Map* map = sMapMgr->FindMap(mapId, 0);
    if (!map)
    {
        LOG_ERROR("server.loading",
            "[LW Creature] Could not find map {} for spawn group {} route node {}.",
            mapId, group.Id, group.RouteNodeId);
        return false;
    }

    uint32 creatureEntry = member.EntityEntry;

    if (member.LwTemplateId != 0)
    {
        creatureEntry = sLwCreatureTemplateMgr.ResolveEntry(member.LwTemplateId);
        if (creatureEntry == 0)
        {
            LOG_ERROR("server.loading",
                "[LW Creature] Runtime #{} spawn member {} references unresolved LW creature template {}.",
                runtimeId,
                member.Id,
                member.LwTemplateId);
            return false;
        }
    }

    if (creatureEntry == 0)
    {
        LOG_ERROR("server.loading",
            "[LW Creature] Runtime #{} spawn member {} has neither a creature entry nor an LW creature template.",
            runtimeId,
            member.Id);
        return false;
    }

    bool spawnedAny = false;

    for (uint32 i = 0; i < member.Count; ++i)
    {
        Position position = BuildSpawnPosition(group, anchorX, anchorY, anchorZ, anchorOrientation);

        TempSummon* summon = map->SummonCreature(
            creatureEntry,
            position,
            nullptr,
            0);

        if (!summon)
        {
            LOG_ERROR("server.loading",
                "[LW Creature] Runtime #{} failed to spawn creature entry {} from member {}.",
                runtimeId,
                creatureEntry,
                member.Id);
            continue;
        }

        if (member.LevelOverride > 0)
        {
            uint8 const requestedLevel = static_cast<uint8>(std::min<uint16>(member.LevelOverride, 255));
            summon->SetLevel(requestedLevel);

            // Recalculate level-dependent creature stats after changing the level.
            // Without this, the displayed level can change while health/mana/damage
            // remain based on the original creature-template level.
            summon->UpdateAllStats();
            summon->SetFullHealth();
            if (summon->GetMaxPower(POWER_MANA) > 0)
            {
                summon->SetPower(POWER_MANA, summon->GetMaxPower(POWER_MANA));
            }

            LOG_INFO("server.loading",
                "[LW Creature] Runtime #{} applied level override {} to creature {} from member {}.",
                runtimeId,
                static_cast<uint32>(requestedLevel),
                creatureEntry,
                member.Id);
        }

        RuntimeEntity entity;
        entity.EntityType = GetType();
        entity.MapId = mapId;
        entity.GroupId = group.Id;
        entity.MemberId = member.Id;
        entity.Entry = creatureEntry;
        entity.LwTemplateId = member.LwTemplateId;
        entity.TacticalRole = static_cast<uint8>(member.Role);
        entity.Guid = summon->GetGUID();
        spawnedEntities.push_back(std::move(entity));

        LOG_INFO("server.loading",
            "[LW Creature] Runtime #{} spawned creature {} from member {} role {} ({}) GUID {}.",
            runtimeId,
            creatureEntry,
            member.Id,
            static_cast<uint32>(member.Role),
            TacticalRoleName(member.Role),
            summon->GetGUID().ToString());

        spawnedAny = true;
    }

    return spawnedAny;
}

bool CreatureProvider::Cleanup(RuntimeEntity const& entity)
{
    Map* map = sMapMgr->FindMap(entity.MapId, 0);
    if (!map)
    {
        return false;
    }

    Creature* creature = map->GetCreature(entity.Guid);
    if (!creature)
    {
        return false;
    }

    creature->DespawnOrUnsummon();
    return true;
}
}
