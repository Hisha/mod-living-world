#include "InvasionSpawnManager.h"

#include "CreatureProvider.h"
#include "GameObjectProvider.h"
#include "Creature.h"
#include "LivingWorld.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "MovementController.h"
#include "Player.h"
#include "RuntimeEntityGroup.h"

#include <algorithm>
#include <utility>

namespace lw
{
InvasionSpawnManager& InvasionSpawnManager::Instance()
{
    static InvasionSpawnManager instance;
    return instance;
}

InvasionSpawnManager::InvasionSpawnManager()
{
    RegisterProvider(std::make_unique<CreatureProvider>());
    RegisterProvider(std::make_unique<GameObjectProvider>());
}

void InvasionSpawnManager::RegisterProvider(std::unique_ptr<IEntityProvider> provider)
{
    if (!provider)
    {
        return;
    }

    uint8 type = provider->GetType();
    _providers[type] = std::move(provider);
}

IEntityProvider* InvasionSpawnManager::GetProvider(uint8 entityType)
{
    auto itr = _providers.find(entityType);
    if (itr == _providers.end())
    {
        return nullptr;
    }

    return itr->second.get();
}

void InvasionSpawnManager::Reset()
{
    _activeGarrisons.clear();
    sRuntimeEntityGroupMgr.Reset();
}

bool InvasionSpawnManager::SpawnGroup(uint64 runtimeId, uint32 spawnGroupId, uint64* runtimeGroupId)
{
    SpawnGroupDefinition const* group = sLivingWorldDataMgr.GetSpawnGroup(spawnGroupId);
    if (!group)
    {
        LOG_ERROR("server.loading", "[LW Spawn] Missing spawn group {}.", spawnGroupId);
        return false;
    }

    auto const* members = sLivingWorldDataMgr.GetSpawnMembers(spawnGroupId);
    if (!members || members->empty())
    {
        LOG_ERROR("server.loading", "[LW Spawn] Spawn group {} has no members.", spawnGroupId);
        return false;
    }

    RuntimeEntityGroup& runtimeGroup = sRuntimeEntityGroupMgr.CreateGroup(runtimeId, spawnGroupId);

    if (runtimeGroupId)
    {
        *runtimeGroupId = runtimeGroup.Id;
    }

    LOG_INFO("server.loading",
        "[LW Spawn] Runtime #{} executing spawn group {} ({}) as runtime entity group #{}.",
        runtimeId,
        spawnGroupId,
        group->Name,
        runtimeGroup.Id);

    bool spawnedAny = false;

    for (SpawnMemberDefinition const& member : *members)
    {
        IEntityProvider* provider = GetProvider(member.EntityType);
        if (!provider)
        {
            LOG_ERROR("server.loading",
                "[LW Spawn] Runtime #{} spawn member {} uses unsupported entity type {}.",
                runtimeId,
                member.Id,
                member.EntityType);
            continue;
        }

        if (provider->Spawn(runtimeId, *group, member, runtimeGroup.Entities))
        {
            spawnedAny = true;
        }
    }

    if (!spawnedAny)
    {
        sRuntimeEntityGroupMgr.RemoveGroup(runtimeGroup.Id);

        if (runtimeGroupId)
        {
            *runtimeGroupId = 0;
        }

        LOG_ERROR("server.loading",
            "[LW Spawn] Runtime #{} spawn group {} created no entities; runtime entity group removed.",
            runtimeId,
            spawnGroupId);

        return false;
    }

    LOG_INFO("server.loading",
        "[LW Spawn] Runtime entity group #{} now owns {} entity/entities for runtime #{} at route node {}.",
        runtimeGroup.Id,
        runtimeGroup.Entities.size(),
        runtimeId,
        group->RouteNodeId);

    return true;
}


bool InvasionSpawnManager::StartGarrison(
    uint64 runtimeId,
    uint32 spawnGroupId,
    uint32 quietPeriodSeconds,
    uint32 refillBatchSize,
    uint32 refillIntervalSeconds)
{
    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.FindLatestGroup(runtimeId, spawnGroupId);
    if (!group)
    {
        LOG_ERROR("server.loading",
            "[LW Garrison] Runtime #{} cannot start garrison behavior because spawn group {} has no runtime entity group.",
            runtimeId,
            spawnGroupId);
        return false;
    }

    ActiveGarrison garrison;
    garrison.RuntimeId = runtimeId;
    garrison.RuntimeGroupId = group->Id;
    garrison.SpawnGroupId = spawnGroupId;
    garrison.QuietPeriodSeconds = quietPeriodSeconds == 0 ? 30 : quietPeriodSeconds;
    garrison.RefillBatchSize = refillBatchSize == 0 ? 5 : refillBatchSize;
    garrison.RefillIntervalSeconds = refillIntervalSeconds == 0 ? 10 : refillIntervalSeconds;

    _activeGarrisons[group->Id] = garrison;

    LOG_INFO("server.loading",
        "[LW Garrison] Runtime #{} runtime entity group #{} (spawn group {}) enabled garrison restock: "
        "{} second quiet period, up to {} replacement(s) per batch, {} second refill interval.",
        runtimeId,
        group->Id,
        spawnGroupId,
        garrison.QuietPeriodSeconds,
        garrison.RefillBatchSize,
        garrison.RefillIntervalSeconds);

    return true;
}

bool InvasionSpawnManager::IsGarrisonQuiet(ActiveGarrison const& garrison) const
{
    RuntimeEntityGroup const* group = sRuntimeEntityGroupMgr.GetGroup(garrison.RuntimeGroupId);
    if (!group || group->State != RuntimeEntityGroupState::Active)
    {
        return false;
    }

    Creature* hostilityReference = nullptr;
    Map* referenceMap = nullptr;

    for (RuntimeEntity const& entity : group->Entities)
    {
        if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
        {
            continue;
        }

        Map* map = sMapMgr->FindMap(entity.MapId, 0);
        if (!map)
        {
            continue;
        }

        Creature* creature = map->GetCreature(entity.Guid);
        if (!creature || !creature->IsAlive())
        {
            continue;
        }

        if (creature->IsInCombat() || creature->GetVictim())
        {
            return false;
        }

        if (!hostilityReference)
        {
            hostilityReference = creature;
            referenceMap = map;
        }
    }

    // A completely destroyed garrison is not magically rebuilt from nothing.
    if (!hostilityReference || !referenceMap)
    {
        return false;
    }

    // Do not restock while a hostile player/playerbot is still close enough to
    // contest the position, even if combat momentarily dropped.
    constexpr float HostilePlayerBlockRadius = 100.0f;

    Map::PlayerList const& players = referenceMap->GetPlayers();
    for (auto const& playerRef : players)
    {
        Player* player = playerRef.GetSource();
        if (!player || !player->IsInWorld())
        {
            continue;
        }

        if (hostilityReference->GetDistance(player) > HostilePlayerBlockRadius)
        {
            continue;
        }

        // IsHostileTo is intentionally used instead of IsValidAttackTarget here.
        // A dead player/playerbot doing a corpse run is still a defending presence
        // and should continue blocking free garrison replacements.
        if (hostilityReference->IsHostileTo(player))
        {
            return false;
        }
    }

    return true;
}

bool InvasionSpawnManager::RefillGarrison(ActiveGarrison& garrison, uint64 nowSeconds)
{
    RuntimeEntityGroup* runtimeGroup = sRuntimeEntityGroupMgr.GetGroup(garrison.RuntimeGroupId);
    if (!runtimeGroup || runtimeGroup->State != RuntimeEntityGroupState::Active)
    {
        return false;
    }

    SpawnGroupDefinition const* originalGroup = sLivingWorldDataMgr.GetSpawnGroup(garrison.SpawnGroupId);
    auto const* members = sLivingWorldDataMgr.GetSpawnMembers(garrison.SpawnGroupId);

    if (!originalGroup || !members || members->empty())
    {
        return false;
    }

    // Anchor replacements around the surviving garrison. Because movement-path
    // completion now promotes each survivor's final formation point to its home,
    // an out-of-combat group naturally settles back around the captured objective.
    double sumX = 0.0;
    double sumY = 0.0;
    double sumZ = 0.0;
    double sumOrientation = 0.0;
    uint32 anchorCount = 0;

    for (RuntimeEntity const& entity : runtimeGroup->Entities)
    {
        if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
        {
            continue;
        }

        Map* map = sMapMgr->FindMap(entity.MapId, 0);
        if (!map)
        {
            continue;
        }

        Creature* creature = map->GetCreature(entity.Guid);
        if (!creature || !creature->IsAlive())
        {
            continue;
        }

        Position const& home = creature->GetHomePosition();
        sumX += home.GetPositionX();
        sumY += home.GetPositionY();
        sumZ += home.GetPositionZ();
        sumOrientation += home.GetOrientation();
        ++anchorCount;
    }

    if (anchorCount == 0)
    {
        return false;
    }

    SpawnGroupDefinition refillGroup = *originalGroup;
    refillGroup.RouteNodeId = 0;
    refillGroup.UseExplicitPosition = true;
    refillGroup.X = static_cast<float>(sumX / anchorCount);
    refillGroup.Y = static_cast<float>(sumY / anchorCount);
    refillGroup.Z = static_cast<float>(sumZ / anchorCount);
    refillGroup.Orientation = static_cast<float>(sumOrientation / anchorCount);

    uint32 remainingBatch = garrison.RefillBatchSize;
    uint32 spawnedTotal = 0;

    for (SpawnMemberDefinition const& member : *members)
    {
        if (remainingBatch == 0)
        {
            break;
        }

        if (member.EntityType != static_cast<uint8>(EntityProviderType::Creature))
        {
            continue;
        }

        uint32 livingForMember = 0;

        for (RuntimeEntity const& entity : runtimeGroup->Entities)
        {
            if (entity.MemberId != member.Id ||
                entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
            {
                continue;
            }

            Map* map = sMapMgr->FindMap(entity.MapId, 0);
            if (!map)
            {
                continue;
            }

            Creature* creature = map->GetCreature(entity.Guid);
            if (creature && creature->IsAlive())
            {
                ++livingForMember;
            }
        }

        if (livingForMember >= member.Count)
        {
            continue;
        }

        uint32 const missing = static_cast<uint32>(member.Count) - livingForMember;
        uint32 const toSpawn = std::min<uint32>(missing, remainingBatch);

        SpawnMemberDefinition replacement = member;
        replacement.Count = static_cast<uint16>(toSpawn);

        IEntityProvider* provider = GetProvider(replacement.EntityType);
        if (!provider)
        {
            continue;
        }

        std::size_t const before = runtimeGroup->Entities.size();
        if (!provider->Spawn(garrison.RuntimeId, refillGroup, replacement, runtimeGroup->Entities))
        {
            continue;
        }

        uint32 const spawned = static_cast<uint32>(runtimeGroup->Entities.size() - before);
        spawnedTotal += spawned;
        remainingBatch = spawned >= remainingBatch ? 0 : remainingBatch - spawned;
    }

    if (spawnedTotal == 0)
    {
        return false;
    }

    garrison.NextRefillAt = nowSeconds + garrison.RefillIntervalSeconds;

    LOG_INFO("server.loading",
        "[LW Garrison] Runtime #{} runtime entity group #{} restocked {} replacement creature(s) near its objective.",
        garrison.RuntimeId,
        garrison.RuntimeGroupId,
        spawnedTotal);

    return true;
}

void InvasionSpawnManager::UpdateGarrisons(uint64 runtimeId, uint64 nowSeconds)
{
    for (auto& [runtimeGroupId, garrison] : _activeGarrisons)
    {
        (void)runtimeGroupId;

        if (garrison.RuntimeId != runtimeId)
        {
            continue;
        }

        RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(garrison.RuntimeGroupId);
        if (!group || group->State != RuntimeEntityGroupState::Active)
        {
            continue;
        }

        // Never replace losses while the force is still marching to its objective.
        if (sMovementController.IsGroupMoving(garrison.RuntimeGroupId))
        {
            garrison.QuietSince = 0;
            garrison.NextRefillAt = 0;
            continue;
        }

        if (!IsGarrisonQuiet(garrison))
        {
            garrison.QuietSince = 0;
            garrison.NextRefillAt = 0;
            continue;
        }

        if (garrison.QuietSince == 0)
        {
            garrison.QuietSince = nowSeconds;
            continue;
        }

        if (nowSeconds < garrison.QuietSince + garrison.QuietPeriodSeconds)
        {
            continue;
        }

        if (garrison.NextRefillAt != 0 && nowSeconds < garrison.NextRefillAt)
        {
            continue;
        }

        if (!RefillGarrison(garrison, nowSeconds))
        {
            // No losses may exist. Avoid reevaluating every runtime tick.
            garrison.NextRefillAt = nowSeconds + garrison.RefillIntervalSeconds;
        }
    }
}

void InvasionSpawnManager::CleanupRuntime(uint64 runtimeId)
{
    for (auto itr = _activeGarrisons.begin(); itr != _activeGarrisons.end();)
    {
        if (itr->second.RuntimeId == runtimeId)
        {
            itr = _activeGarrisons.erase(itr);
        }
        else
        {
            ++itr;
        }
    }

    std::vector<uint64> groupIds = sRuntimeEntityGroupMgr.GetGroupsForRuntime(runtimeId);
    if (groupIds.empty())
    {
        return;
    }

    uint32 cleaned = 0;
    uint32 tracked = 0;
    uint32 groupsCleaned = 0;

    for (uint64 groupId : groupIds)
    {
        RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(groupId);
        if (!group)
        {
            continue;
        }

        group->State = RuntimeEntityGroupState::CleaningUp;
        tracked += static_cast<uint32>(group->Entities.size());

        for (RuntimeEntity const& entity : group->Entities)
        {
            IEntityProvider* provider = GetProvider(entity.EntityType);
            if (!provider)
            {
                LOG_ERROR("server.loading",
                    "[LW Spawn] Runtime #{} cannot clean entity GUID {} because provider type {} is not registered.",
                    runtimeId,
                    entity.Guid.ToString(),
                    entity.EntityType);
                continue;
            }

            if (provider->Cleanup(entity))
            {
                ++cleaned;
            }
        }

        group->State = RuntimeEntityGroupState::Completed;
        ++groupsCleaned;
    }

    sRuntimeEntityGroupMgr.RemoveRuntime(runtimeId);

    LOG_INFO("server.loading",
        "[LW Spawn] Cleaned runtime #{}. Removed {} of {} tracked entity/entities across {} runtime entity group(s).",
        runtimeId,
        cleaned,
        tracked,
        groupsCleaned);
}
}
