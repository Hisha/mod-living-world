#include "GroupDefeatWatcher.h"

#include "Creature.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "RuntimeEntityGroup.h"
#include "RuntimeSignalManager.h"

#include <algorithm>

namespace lw
{
namespace
{
constexpr uint32 UpdateIntervalMs = 1000;
}

GroupDefeatWatcher& GroupDefeatWatcher::Instance()
{
    static GroupDefeatWatcher instance;
    return instance;
}

void GroupDefeatWatcher::Reset()
{
    _watches.clear();
    _updateTimerMs = 0;
}

bool GroupDefeatWatcher::RegisterWatch(
    uint64 runtimeId,
    uint32 spawnGroupId,
    uint32 signalId,
    bool requireAll)
{
    if (signalId == 0 || spawnGroupId == 0)
    {
        LOG_ERROR("server.loading",
            "[LW Defeat] Runtime #{} cannot register defeat watch with spawn group {} and signal {}.",
            runtimeId, spawnGroupId, signalId);
        return false;
    }

    GroupDefeatWatch& watch = _watches[runtimeId];

    if (watch.RuntimeId == 0)
    {
        watch.RuntimeId = runtimeId;
        watch.SignalId = signalId;
        watch.RequireAll = requireAll;
    }
    else if (watch.SignalId != signalId || watch.RequireAll != requireAll)
    {
        LOG_ERROR("server.loading",
            "[LW Defeat] Runtime #{} attempted to mix defeat-watch configuration "
            "(existing signal {}, require_all {}; new signal {}, require_all {}).",
            runtimeId, watch.SignalId, watch.RequireAll, signalId, requireAll);
        return false;
    }

    if (std::find(watch.SpawnGroupIds.begin(), watch.SpawnGroupIds.end(), spawnGroupId) ==
        watch.SpawnGroupIds.end())
    {
        watch.SpawnGroupIds.push_back(spawnGroupId);
    }

    LOG_INFO("server.loading",
        "[LW Defeat] Runtime #{} now watching spawn group {} for defeat; signal {}, mode {}.",
        runtimeId, spawnGroupId, signalId, requireAll ? "ALL" : "ANY");

    return true;
}

void GroupDefeatWatcher::Update(uint32 diff)
{
    if (_updateTimerMs > diff)
    {
        _updateTimerMs -= diff;
        return;
    }

    _updateTimerMs = UpdateIntervalMs;
    std::vector<uint64> triggered;

    for (auto& [runtimeId, watch] : _watches)
    {
        if (watch.Triggered || watch.SpawnGroupIds.empty())
        {
            continue;
        }

        uint32 defeatedCount = 0;

        for (uint32 spawnGroupId : watch.SpawnGroupIds)
        {
            if (IsSpawnGroupDefeated(runtimeId, spawnGroupId))
            {
                ++defeatedCount;
            }
        }

        bool const satisfied = watch.RequireAll
            ? defeatedCount == watch.SpawnGroupIds.size()
            : defeatedCount != 0;

        if (!satisfied)
        {
            continue;
        }

        watch.Triggered = true;

        LOG_INFO("server.loading",
            "[LW Defeat] Runtime #{} defeat watch satisfied ({}/{} watched group(s) defeated). "
            "Emitting signal {}.",
            runtimeId, defeatedCount, watch.SpawnGroupIds.size(), watch.SignalId);

        sRuntimeSignalMgr.EmitSignal(runtimeId, watch.SignalId);
        triggered.push_back(runtimeId);
    }

    for (uint64 runtimeId : triggered)
    {
        _watches.erase(runtimeId);
    }
}

bool GroupDefeatWatcher::IsSpawnGroupDefeated(uint64 runtimeId, uint32 spawnGroupId) const
{
    bool foundAnyRuntimeGroup = false;

    for (uint64 runtimeGroupId : sRuntimeEntityGroupMgr.GetGroupsForRuntime(runtimeId))
    {
        RuntimeEntityGroup const* group = sRuntimeEntityGroupMgr.GetGroup(runtimeGroupId);
        if (!group || group->SpawnGroupId != spawnGroupId)
        {
            continue;
        }

        foundAnyRuntimeGroup = true;

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
            if (creature && creature->IsAlive())
            {
                return false;
            }
        }
    }

    return foundAnyRuntimeGroup;
}

void GroupDefeatWatcher::CancelRuntime(uint64 runtimeId)
{
    if (_watches.erase(runtimeId) != 0)
    {
        LOG_INFO("server.loading",
            "[LW Defeat] Cancelled defeat watch for runtime #{}.",
            runtimeId);
    }
}
}
