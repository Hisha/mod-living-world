#include "InvasionRuntime.h"

#include "Log.h"
#include "AnnouncementManager.h"
#include "AssaultManager.h"
#include "GroupDefeatWatcher.h"
#include "DialogueManager.h"
#include "InvasionSpawnManager.h"
#include "MovementController.h"
#include "RuntimeEntityGroup.h"
#include "RuntimeSignalManager.h"
#include "SoundManager.h"
#include "SpellActionManager.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace lw
{
namespace
{
constexpr uint8 TimerCompletionType = 0;
constexpr uint8 SignalCompletionType = 1;
constexpr uint8 SpawnGroupActionType = 1;
constexpr uint8 StartMovementActionType = 2;
constexpr uint8 DialogueActionType = 3;
constexpr uint8 WorldAnnouncementActionType = 4;
constexpr uint8 SoundActionType = 5;
constexpr uint8 SpellActionType = 6;
constexpr uint8 StartAssaultActionType = 7;
constexpr uint8 WatchGroupDefeatActionType = 8;
constexpr uint8 StartGarrisonActionType = 9;
}

InvasionRuntime::InvasionRuntime(uint64 runtimeId, uint32 invasionId,
    std::vector<InvasionStageDefinition> stages, uint64 startedAt)
    : _runtimeId(runtimeId),
      _invasionId(invasionId),
      _stages(std::move(stages)),
      _startedAt(startedAt)
{
    std::sort(_stages.begin(), _stages.end(), [](InvasionStageDefinition const& left,
        InvasionStageDefinition const& right)
    {
        return left.StageOrder < right.StageOrder;
    });
}

InvasionRuntime InvasionRuntime::Restore(uint64 runtimeId, uint32 invasionId,
    std::vector<InvasionStageDefinition> stages, uint32 currentStageId,
    uint64 stageStartedAt, uint64 stageEndsAt, uint64 startedAt)
{
    InvasionRuntime runtime(runtimeId, invasionId, std::move(stages), startedAt);
    std::size_t const index = runtime.FindStageIndex(currentStageId);

    if (index >= runtime._stages.size())
    {
        runtime._state = ActiveRuntimeState::Failed;
        return runtime;
    }

    runtime._currentStageIndex = index;
    runtime._stageStartedAt = stageStartedAt;
    runtime._stageEndsAt = stageEndsAt;
    runtime._state = ActiveRuntimeState::Running;
    return runtime;
}

bool InvasionRuntime::Start(uint64 now)
{
    if (_stages.empty())
    {
        _state = ActiveRuntimeState::Failed;
        LOG_ERROR("server.loading", "[LW Runtime] Runtime #{} for invasion {} has no stages.",
            _runtimeId, _invasionId);
        return false;
    }

    _currentStageIndex = 0;
    _startedAt = now;
    _state = ActiveRuntimeState::Running;
    return BeginCurrentStage(now);
}

bool InvasionRuntime::Update(uint64 now)
{
    if (_state != ActiveRuntimeState::Running)
    {
        return _state == ActiveRuntimeState::Completed;
    }

    sInvasionSpawnMgr.UpdateGarrisons(_runtimeId, now);

    InvasionStageDefinition const* stage = GetCurrentStage();
    if (!stage)
    {
        _state = ActiveRuntimeState::Failed;
        return false;
    }

    switch (stage->CompletionType)
    {
        case TimerCompletionType:
            if (now < _stageEndsAt)
            {
                return false;
            }
            return Advance(now);

        case SignalCompletionType:
            if (!sRuntimeSignalMgr.HasSignal(_runtimeId, stage->CompletionTargetId))
            {
                return false;
            }

            LOG_INFO("server.loading",
                "[LW Runtime] Runtime #{} stage {} ({}) satisfied by signal {}.",
                _runtimeId, stage->StageOrder, stage->Name, stage->CompletionTargetId);
            return Advance(now);

        default:
            return false;
    }
}

bool InvasionRuntime::Advance(uint64 now)
{
    if (_state != ActiveRuntimeState::Running)
    {
        return false;
    }

    ++_currentStageIndex;
    if (_currentStageIndex >= _stages.size())
    {
        _state = ActiveRuntimeState::Completed;
        _stageStartedAt = 0;
        _stageEndsAt = 0;

        LOG_INFO("server.loading", "[LW Runtime] Runtime #{} for invasion {} completed all stages.",
            _runtimeId, _invasionId);

        sMovementController.CancelRuntime(_runtimeId);
        sAssaultManager.CancelRuntime(_runtimeId);
        sInvasionSpawnMgr.CleanupRuntime(_runtimeId);
        return true;
    }

    return BeginCurrentStage(now) && false;
}

bool InvasionRuntime::BeginCurrentStage(uint64 now)
{
    InvasionStageDefinition const* stage = GetCurrentStage();
    if (!stage)
    {
        _state = ActiveRuntimeState::Failed;
        return false;
    }

    _stageStartedAt = now;
    _stageEndsAt = stage->CompletionType == TimerCompletionType
        ? now + std::max<uint32>(1, stage->DurationSeconds)
        : 0;

    if (auto const* actions = sLivingWorldDataMgr.GetActions(stage->Id))
    {
        for (auto const& action : *actions)
        {
            if (action.ActionType == SpawnGroupActionType)
            {
                uint64 runtimeGroupId = 0;
                if (sInvasionSpawnMgr.SpawnGroup(_runtimeId, action.TargetId, &runtimeGroupId) && runtimeGroupId != 0)
                {
                    LOG_INFO("server.loading",
                        "[LW StageTrace] Runtime #{} stage {} action {} SPAWNED spawnGroup={} as runtimeGroup=#{}.",
                        _runtimeId, stage->Id, action.Id, action.TargetId, runtimeGroupId);

                    SpawnGroupDefinition const* spawnGroup = sLivingWorldDataMgr.GetSpawnGroup(action.TargetId);
                    if (spawnGroup && spawnGroup->RouteNodeId != 0)
                        sMovementController.NotifyRouteNodeReached(runtimeGroupId, spawnGroup->RouteNodeId, _invasionId);
                }
                continue;
            }

            if (action.ActionType == StartMovementActionType)
            {
                RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.FindLatestGroup(_runtimeId, action.TargetId);
                if (!group)
                {
                    LOG_ERROR("server.loading",
                        "[LW Runtime] Runtime #{} cannot start movement action {} because spawn group {} has no runtime entity group.",
                        _runtimeId, action.Id, action.TargetId);
                    continue;
                }

                LOG_INFO("server.loading",
                    "[LW StageTrace] Runtime #{} stage {} action {} RELEASE movement spawnGroup={} runtimeGroup=#{} "
                    "routeNode {} -> {} completionSignal={}.",
                    _runtimeId, stage->Id, action.Id, action.TargetId, group->Id,
                    action.Parameter1, action.Parameter2, action.Parameter3);

                if (!sMovementController.StartRouteJourney(
                        group->Id,
                        action.Parameter1,
                        action.Parameter2,
                        0,
                        action.Parameter3))
                {
                    LOG_ERROR("server.loading",
                        "[LW Runtime] Runtime #{} failed route movement action {} for runtime entity group #{} "
                        "(start route node {}, destination route node {}, completion signal {}).",
                        _runtimeId, action.Id, group->Id, action.Parameter1, action.Parameter2, action.Parameter3);
                }
            }

            if (action.ActionType == DialogueActionType)
            {
                if (!sDialogueManager.Execute(_runtimeId, action.TargetId, action.Parameter1, action.Parameter2))
                {
                    LOG_ERROR("server.loading",
                        "[LW Runtime] Runtime #{} failed dialogue action {} (spawn group {}, dialogue {}, speaker member {}).",
                        _runtimeId, action.Id, action.TargetId, action.Parameter1, action.Parameter2);
                }
            }

            if (action.ActionType == WorldAnnouncementActionType)
            {
                if (!sAnnouncementManager.Execute(
                    _runtimeId,
                    _invasionId,
                    action.TargetId,
                    action.Parameter1,
                    action.Parameter2,
                    action.Parameter3))
                {
                    LOG_ERROR("server.loading",
                        "[LW Runtime] Runtime #{} failed world announcement action {} "
                        "(announcement {}, scope {}, scope id {}, faction {}).",
                        _runtimeId,
                        action.Id,
                        action.TargetId,
                        action.Parameter1,
                        action.Parameter2,
                        action.Parameter3);
                }
            }

            if (action.ActionType == SoundActionType)
            {
                if (!sSoundManager.Execute(
                    _runtimeId,
                    action.TargetId,
                    action.Parameter1,
                    action.Parameter2,
                    action.Parameter3))
                {
                    LOG_ERROR("server.loading",
                        "[LW Runtime] Runtime #{} failed sound action {} "
                        "(spawn group {}, sound {}, source member {}, mode {}).",
                        _runtimeId,
                        action.Id,
                        action.TargetId,
                        action.Parameter1,
                        action.Parameter2,
                        action.Parameter3);
                }
            }

            if (action.ActionType == SpellActionType)
            {
                if (!sSpellActionManager.ExecuteSelfCast(
                    _runtimeId,
                    action.TargetId,
                    action.Parameter1,
                    action.Parameter2,
                    action.Parameter3))
                {
                    LOG_ERROR("server.loading",
                        "[LW Runtime] Runtime #{} failed spell action {} "
                        "(spawn group {}, spell {}, caster member {}, target mode {}).",
                        _runtimeId,
                        action.Id,
                        action.TargetId,
                        action.Parameter1,
                        action.Parameter2,
                        action.Parameter3);
                }
            }

            if (action.ActionType == StartAssaultActionType)
            {
                if (!sAssaultManager.Start(
                    _runtimeId,
                    action.TargetId,
                    action.Parameter1,
                    action.Parameter2,
                    action.Parameter3,
                    action.Parameter4))
                {
                    LOG_ERROR("server.loading",
                        "[LW Runtime] Runtime #{} failed assault action {} "
                        "(spawn group {}, radius {}, reacquire interval {}, target policy {}, assault center route node {}).",
                        _runtimeId,
                        action.Id,
                        action.TargetId,
                        action.Parameter1,
                        action.Parameter2,
                        action.Parameter3,
                        action.Parameter4);
                }
            }

            if (action.ActionType == WatchGroupDefeatActionType)
            {
                if (!sGroupDefeatWatcher.RegisterWatch(
                    _runtimeId,
                    action.TargetId,
                    action.Parameter1,
                    action.Parameter2 != 0))
                {
                    LOG_ERROR("server.loading",
                        "[LW Runtime] Runtime #{} failed defeat-watch action {} "
                        "(spawn group {}, signal {}, require all {}).",
                        _runtimeId,
                        action.Id,
                        action.TargetId,
                        action.Parameter1,
                        action.Parameter2 != 0);
                }
            }

            if (action.ActionType == StartGarrisonActionType)
            {
                if (!sInvasionSpawnMgr.StartGarrison(
                    _runtimeId,
                    action.TargetId,
                    action.Parameter1,
                    action.Parameter2,
                    action.Parameter3))
                {
                    LOG_ERROR("server.loading",
                        "[LW Runtime] Runtime #{} failed garrison action {} "
                        "(spawn group {}, quiet period {} sec, batch size {}, refill interval {} sec).",
                        _runtimeId,
                        action.Id,
                        action.TargetId,
                        action.Parameter1,
                        action.Parameter2,
                        action.Parameter3);
                }
            }
        }
    }

    if (stage->CompletionType == SignalCompletionType)
    {
        RuntimeSignalDefinition const* signal = sLivingWorldDataMgr.GetRuntimeSignal(stage->CompletionTargetId);
        LOG_INFO("server.loading",
            "[LW Runtime] Runtime #{} invasion {} entered stage {} ({}) waiting for signal {} ({}).",
            _runtimeId, _invasionId, stage->StageOrder, stage->Name,
            stage->CompletionTargetId, signal ? signal->Name : "unknown");
    }
    else
    {
        LOG_INFO("server.loading",
            "[LW Runtime] Runtime #{} invasion {} entered stage {} ({}) for {} second(s).",
            _runtimeId, _invasionId, stage->StageOrder, stage->Name,
            std::max<uint32>(1, stage->DurationSeconds));
    }

    return true;
}

std::size_t InvasionRuntime::FindStageIndex(uint32 stageId) const
{
    for (std::size_t index = 0; index < _stages.size(); ++index)
    {
        if (_stages[index].Id == stageId)
        {
            return index;
        }
    }

    return _stages.size();
}

uint64 InvasionRuntime::GetRuntimeId() const { return _runtimeId; }
uint32 InvasionRuntime::GetInvasionId() const { return _invasionId; }
ActiveRuntimeState InvasionRuntime::GetState() const { return _state; }

InvasionStageDefinition const* InvasionRuntime::GetCurrentStage() const
{
    return _currentStageIndex < _stages.size() ? &_stages[_currentStageIndex] : nullptr;
}

uint64 InvasionRuntime::GetStageStartedAt() const { return _stageStartedAt; }
uint64 InvasionRuntime::GetStageEndsAt() const { return _stageEndsAt; }
uint64 InvasionRuntime::GetStartedAt() const { return _startedAt; }

uint64 InvasionRuntime::GetStageTimeRemaining(uint64 now) const
{
    return _stageEndsAt > now ? _stageEndsAt - now : 0;
}

std::string InvasionRuntime::BuildStatusLine(uint64 now) const
{
    std::ostringstream output;
    output << "Runtime #" << _runtimeId;

    if (InvasionStageDefinition const* stage = GetCurrentStage())
    {
        output << ", stage " << stage->StageOrder << " (" << stage->Name << ")";

        if (stage->CompletionType == SignalCompletionType)
        {
            output << ", waiting for signal " << stage->CompletionTargetId;
            if (RuntimeSignalDefinition const* signal = sLivingWorldDataMgr.GetRuntimeSignal(stage->CompletionTargetId))
            {
                output << " (" << signal->Name << ")";
            }
        }
        else
        {
            output << ", " << GetStageTimeRemaining(now) << " second(s) remaining";
        }
    }
    else
    {
        output << ", no active stage";
    }

    return output.str();
}
}
