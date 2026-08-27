#include "InvasionRuntimeManager.h"

#include "InvasionScheduler.h"
#include "AssaultManager.h"
#include "GroupDefeatWatcher.h"
#include "CreatureAbilityManager.h"
#include "LivingWorld.h"
#include "MovementController.h"
#include "InvasionSpawnManager.h"
#include "RuntimeSignalManager.h"

#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"

#include <ctime>
#include <sstream>
#include <vector>

namespace lw
{
namespace
{
constexpr uint32 UpdateIntervalMs = 1000;

uint64 UnixTimeNow()
{
    return static_cast<uint64>(std::time(nullptr));
}
}

InvasionRuntimeManager& InvasionRuntimeManager::Instance()
{
    static InvasionRuntimeManager instance;
    return instance;
}

void InvasionRuntimeManager::Reset()
{
    sMovementController.Reset();
    sAssaultManager.Reset();
    sGroupDefeatWatcher.Reset();
    sCreatureAbilityMgr.Reset();
    sRuntimeSignalMgr.Reset();
    _runtimes.clear();
    _runtimeByInvasion.clear();
    _updateTimerMs = 0;
    _runtimeSequence = 0;
    _initialized = false;
}

void InvasionRuntimeManager::Initialize()
{
    Reset();
    uint64 const now = UnixTimeNow();
    LoadActiveRuntimes(now);
    sCreatureAbilityMgr.Load();
    _updateTimerMs = UpdateIntervalMs;
    _initialized = true;

    LOG_INFO("server.loading", "[LW Runtime] Runtime manager initialized with {} active runtime(s).",
        _runtimes.size());
}

void InvasionRuntimeManager::Update(uint32 diff)
{
    if (!_initialized)
    {
        return;
    }

    sMovementController.Update(diff);
    sAssaultManager.Update(diff);
    sGroupDefeatWatcher.Update(diff);
    sCreatureAbilityMgr.Update(diff);

    if (_updateTimerMs > diff)
    {
        _updateTimerMs -= diff;
        return;
    }

    _updateTimerMs = UpdateIntervalMs;
    uint64 const now = UnixTimeNow();
    std::vector<uint64> completed;
    std::vector<uint64> timedOut;

    for (auto& [runtimeId, runtime] : _runtimes)
    {
        InvasionDefinition const* definition = sLivingWorldDataMgr.GetDefinition(runtime.GetInvasionId());
        if (definition &&
            definition->MaximumRuntimeSeconds != 0 &&
            runtime.GetStartedAt() != 0 &&
            now >= runtime.GetStartedAt() + definition->MaximumRuntimeSeconds)
        {
            timedOut.push_back(runtimeId);
            continue;
        }

        if (runtime.Update(now))
        {
            completed.push_back(runtimeId);
        }
        else if (runtime.GetState() == ActiveRuntimeState::Running)
        {
            SaveRuntime(runtime);
        }
    }

    for (uint64 runtimeId : completed)
    {
        CompleteRuntime(runtimeId, now);
    }

    for (uint64 runtimeId : timedOut)
    {
        TimeoutRuntime(runtimeId, now);
    }
}

bool InvasionRuntimeManager::StartInvasion(uint32 invasionId)
{
    if (!_initialized || _runtimeByInvasion.find(invasionId) != _runtimeByInvasion.end())
    {
        return false;
    }

    std::vector<InvasionStageDefinition> const* stages = sLivingWorldDataMgr.GetStages(invasionId);
    if (!stages || stages->empty())
    {
        LOG_ERROR("server.loading", "[LW Runtime] Invasion {} cannot start because it has no enabled stages.", invasionId);
        return false;
    }

    uint64 const now = UnixTimeNow();
    uint64 const runtimeId = GenerateRuntimeId(now);
    InvasionRuntime runtime(runtimeId, invasionId, *stages, now);

    if (!runtime.Start(now))
    {
        return false;
    }

    SaveRuntime(runtime);
    _runtimeByInvasion[invasionId] = runtimeId;
    _runtimes.emplace(runtimeId, std::move(runtime));

    LOG_INFO("server.loading", "[LW Runtime] Started runtime #{} for invasion {}.", runtimeId, invasionId);
    return true;
}

bool InvasionRuntimeManager::AdvanceRuntime(uint64 runtimeId)
{
    auto iterator = _runtimes.find(runtimeId);
    if (iterator == _runtimes.end())
    {
        return false;
    }

    uint64 const now = UnixTimeNow();
    if (iterator->second.Advance(now))
    {
        CompleteRuntime(runtimeId, now);
        return true;
    }

    SaveRuntime(iterator->second);
    return true;
}

bool InvasionRuntimeManager::FailRuntime(uint64 runtimeId, char const* reason)
{
    auto iterator = _runtimes.find(runtimeId);
    if (iterator == _runtimes.end())
    {
        return false;
    }

    uint64 const now = UnixTimeNow();
    uint32 const invasionId = iterator->second.GetInvasionId();
    InvasionDefinition const* definition = sLivingWorldDataMgr.GetDefinition(invasionId);

    LOG_INFO("server.loading",
        "[LW Runtime] Runtime #{} for invasion {} ({}) failed: {}.",
        runtimeId,
        invasionId,
        definition ? definition->Name : "unknown",
        reason ? reason : "unspecified failure");

    sMovementController.CancelRuntime(runtimeId);
    sCreatureAbilityMgr.CancelRuntime(runtimeId);
    sAssaultManager.CancelRuntime(runtimeId);
    sGroupDefeatWatcher.CancelRuntime(runtimeId);
    sInvasionSpawnMgr.CleanupRuntime(runtimeId);
    sRuntimeSignalMgr.ClearRuntime(runtimeId);
    DeleteRuntime(runtimeId);

    _runtimeByInvasion.erase(invasionId);
    _runtimes.erase(iterator);

    sInvasionScheduler.NotifyInvasionFailed(invasionId, now);
    return true;
}

void InvasionRuntimeManager::AbortAll()
{
    if (_runtimes.empty())
    {
        LOG_INFO("server.loading", "[LW Runtime] Abort requested with no active runtimes.");
        return;
    }

    std::vector<uint64> runtimeIds;
    runtimeIds.reserve(_runtimes.size());

    for (auto const& [runtimeId, runtime] : _runtimes)
    {
        (void)runtime;
        runtimeIds.push_back(runtimeId);
    }

    uint32 aborted = 0;

    for (uint64 runtimeId : runtimeIds)
    {
        auto iterator = _runtimes.find(runtimeId);
        if (iterator == _runtimes.end())
        {
            continue;
        }

        uint32 const invasionId = iterator->second.GetInvasionId();

        LOG_INFO("server.loading",
            "[LW Runtime] Emergency abort of runtime #{} for invasion {}.",
            runtimeId, invasionId);

        sMovementController.CancelRuntime(runtimeId);
        sCreatureAbilityMgr.CancelRuntime(runtimeId);
        sAssaultManager.CancelRuntime(runtimeId);
        sGroupDefeatWatcher.CancelRuntime(runtimeId);
        sInvasionSpawnMgr.CleanupRuntime(runtimeId);
        sRuntimeSignalMgr.ClearRuntime(runtimeId);
        DeleteRuntime(runtimeId);

        _runtimeByInvasion.erase(invasionId);
        _runtimes.erase(iterator);

        // Return the scheduler record to Available instead of treating an
        // emergency abort as a successful invasion completion.
        sInvasionScheduler.NotifyInvasionStartFailed(invasionId);
        ++aborted;
    }

    LOG_INFO("server.loading", "[LW Runtime] Emergency abort completed for {} runtime(s).", aborted);
}

uint32 InvasionRuntimeManager::GetActiveRuntimeCount() const
{
    return static_cast<uint32>(_runtimes.size());
}

void InvasionRuntimeManager::LoadActiveRuntimes(uint64 now)
{
    if (QueryResult result = CharacterDatabase.Query(
        "SELECT `runtime_id`, `invasion_id`, `current_stage_id`, `stage_started_at`, `stage_ends_at`, `started_at` "
        "FROM `lw_active_runtime` ORDER BY `runtime_id`"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint64 const runtimeId = fields[0].Get<uint64>();
            uint32 const invasionId = fields[1].Get<uint32>();
            uint32 const currentStageId = fields[2].Get<uint32>();
            uint64 const stageStartedAt = fields[3].Get<uint64>();
            uint64 const stageEndsAt = fields[4].Get<uint64>();
            uint64 const startedAt = fields[5].Get<uint64>();

            std::vector<InvasionStageDefinition> const* stages = sLivingWorldDataMgr.GetStages(invasionId);
            if (!stages || stages->empty() || !sInvasionScheduler.IsInvasionActive(invasionId))
            {
                CharacterDatabase.Execute("DELETE FROM `lw_active_runtime` WHERE `runtime_id` = {}", runtimeId);
                if (sInvasionScheduler.IsInvasionActive(invasionId))
                {
                    sInvasionScheduler.NotifyInvasionStartFailed(invasionId);
                }
                continue;
            }

            InvasionRuntime runtime = InvasionRuntime::Restore(runtimeId, invasionId, *stages,
                currentStageId, stageStartedAt, stageEndsAt, startedAt);

            if (runtime.GetState() == ActiveRuntimeState::Failed)
            {
                CharacterDatabase.Execute("DELETE FROM `lw_active_runtime` WHERE `runtime_id` = {}", runtimeId);
                sInvasionScheduler.NotifyInvasionStartFailed(invasionId);
                continue;
            }

            _runtimeByInvasion[invasionId] = runtimeId;
            _runtimes.emplace(runtimeId, std::move(runtime));

            LOG_INFO("server.loading", "[LW Runtime] Restored runtime #{} for invasion {}.", runtimeId, invasionId);
        } while (result->NextRow());
    }

    for (uint32 invasionId : sInvasionScheduler.GetActiveInvasionIds())
    {
        if (_runtimeByInvasion.find(invasionId) == _runtimeByInvasion.end())
        {
            LOG_ERROR("server.loading", "[LW Runtime] Scheduler marked invasion {} active, but no runtime row exists; resetting it.",
                invasionId);
            sInvasionScheduler.NotifyInvasionStartFailed(invasionId);
        }
    }

    (void)now;
}

void InvasionRuntimeManager::SaveRuntime(InvasionRuntime const& runtime)
{
    InvasionStageDefinition const* stage = runtime.GetCurrentStage();
    if (!stage)
    {
        return;
    }

    CharacterDatabase.Execute(
        "REPLACE INTO `lw_active_runtime` "
        "(`runtime_id`, `invasion_id`, `current_stage_id`, `stage_started_at`, `stage_ends_at`, `started_at`) "
        "VALUES ({}, {}, {}, {}, {}, {})",
        runtime.GetRuntimeId(), runtime.GetInvasionId(), stage->Id,
        runtime.GetStageStartedAt(), runtime.GetStageEndsAt(), runtime.GetStartedAt());
}

void InvasionRuntimeManager::DeleteRuntime(uint64 runtimeId)
{
    CharacterDatabase.Execute("DELETE FROM `lw_active_runtime` WHERE `runtime_id` = {}", runtimeId);
}

void InvasionRuntimeManager::CompleteRuntime(uint64 runtimeId, uint64 now)
{
    auto iterator = _runtimes.find(runtimeId);
    if (iterator == _runtimes.end())
    {
        return;
    }

    uint32 const invasionId = iterator->second.GetInvasionId();
    sRuntimeSignalMgr.ClearRuntime(runtimeId);
    DeleteRuntime(runtimeId);
    _runtimeByInvasion.erase(invasionId);
    _runtimes.erase(iterator);
    sInvasionScheduler.NotifyInvasionCompleted(invasionId, now);
}

void InvasionRuntimeManager::TimeoutRuntime(uint64 runtimeId, uint64 now)
{
    auto iterator = _runtimes.find(runtimeId);
    if (iterator == _runtimes.end())
    {
        return;
    }

    uint32 const invasionId = iterator->second.GetInvasionId();
    InvasionDefinition const* definition = sLivingWorldDataMgr.GetDefinition(invasionId);

    LOG_ERROR("server.loading",
        "[LW Runtime] Runtime #{} for invasion {} ({}) exceeded its maximum lifetime of {} second(s). Forcing cleanup.",
        runtimeId,
        invasionId,
        definition ? definition->Name : "unknown",
        definition ? definition->MaximumRuntimeSeconds : 0);

    sMovementController.CancelRuntime(runtimeId);
    sCreatureAbilityMgr.CancelRuntime(runtimeId);
    sAssaultManager.CancelRuntime(runtimeId);
    sGroupDefeatWatcher.CancelRuntime(runtimeId);
    sInvasionSpawnMgr.CleanupRuntime(runtimeId);
    sRuntimeSignalMgr.ClearRuntime(runtimeId);
    DeleteRuntime(runtimeId);

    _runtimeByInvasion.erase(invasionId);
    _runtimes.erase(iterator);

    sInvasionScheduler.NotifyInvasionTimedOut(invasionId, now);
}

uint64 InvasionRuntimeManager::GenerateRuntimeId(uint64 now)
{
    ++_runtimeSequence;
    return now * 1000ULL + (_runtimeSequence % 1000U);
}

InvasionRuntime const* InvasionRuntimeManager::GetRuntimeForInvasion(uint32 invasionId) const
{
    auto idIterator = _runtimeByInvasion.find(invasionId);
    return idIterator != _runtimeByInvasion.end() ? GetRuntime(idIterator->second) : nullptr;
}

InvasionRuntime const* InvasionRuntimeManager::GetRuntime(uint64 runtimeId) const
{
    auto iterator = _runtimes.find(runtimeId);
    return iterator != _runtimes.end() ? &iterator->second : nullptr;
}

std::string InvasionRuntimeManager::BuildStatusReport() const
{
    std::ostringstream output;
    uint64 const now = UnixTimeNow();
    output << "\nActive runtimes:\n";

    if (_runtimes.empty())
    {
        output << "  None.\n";
        return output.str();
    }

    for (auto const& [runtimeId, runtime] : _runtimes)
    {
        (void)runtimeId;
        output << "  " << runtime.BuildStatusLine(now);

        if (InvasionDefinition const* definition = sLivingWorldDataMgr.GetDefinition(runtime.GetInvasionId()))
        {
            if (definition->MaximumRuntimeSeconds != 0 && runtime.GetStartedAt() != 0)
            {
                uint64 const deadline = runtime.GetStartedAt() + definition->MaximumRuntimeSeconds;
                uint64 const remaining = deadline > now ? deadline - now : 0;
                output << ", hard timeout in " << remaining << "s";
            }
        }

        output << '\n';
    }

    return output.str();
}
}
