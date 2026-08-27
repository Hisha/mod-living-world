#include "InvasionScheduler.h"

#include "LivingWorld.h"
#include "InvasionRuntimeManager.h"

#include "Config.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"
#include "Random.h"

#include <algorithm>
#include <ctime>
#include <limits>
#include <sstream>
#include <string>

namespace lw
{

namespace
{

constexpr uint32 MillisecondsPerSecond = 1000;

uint64 UnixTimeNow()
{
    return static_cast<uint64>(std::time(nullptr));
}

uint32 RandomBetween(uint32 minimum, uint32 maximum)
{
    if (maximum < minimum)
    {
        std::swap(minimum, maximum);
    }

    return minimum == maximum ? minimum : urand(minimum, maximum);
}

}

InvasionScheduler& InvasionScheduler::Instance()
{
    static InvasionScheduler instance;
    return instance;
}

void InvasionScheduler::Configure(SchedulerSettings settings)
{
    _settings = settings;
}

void InvasionScheduler::Pause()
{
    _controlState = SchedulerControlState::Paused;

    LOG_INFO("server.loading",
        "[LW Scheduler] Scheduler paused. Existing invasions will continue.");
}

void InvasionScheduler::Resume()
{
    _controlState = SchedulerControlState::Running;

    LOG_INFO("server.loading",
        "[LW Scheduler] Scheduler resumed.");
}

void InvasionScheduler::Drain()
{
    _controlState = SchedulerControlState::Draining;

    LOG_INFO("server.loading",
        "[LW Scheduler] Scheduler draining. Waiting for active invasions to complete.");
}

SchedulerControlState InvasionScheduler::GetControlState() const
{
    return _controlState;
}


bool InvasionScheduler::TriggerInvasion(uint32 invasionId)
{
    if (!_initialized || !_settings.Enabled || _controlState != SchedulerControlState::Running)
    {
        return false;
    }

    if (!sLivingWorldDataMgr.GetDefinition(invasionId) || IsInvasionActive(invasionId))
    {
        return false;
    }

    uint64 const now = UnixTimeNow();
    LOG_INFO("server.loading", "[LW Scheduler] Debug trigger requested for invasion {}.", invasionId);
    return StartInvasion(invasionId, now);
}

void InvasionScheduler::Reset()
{
    _runtime.clear();
    _nextMapEvaluation.clear();
    _updateTimerMs = 0;
    _initialized = false;
    _controlState = SchedulerControlState::Running;
}

void InvasionScheduler::Initialize()
{
    Reset();

    if (!_settings.Enabled)
    {
        LOG_INFO("server.loading", "Living World Scheduler is disabled.");
        return;
    }

    LoadRuntimeState();
    EnsureRuntimeRows();

    uint64 const now = UnixTimeNow();
    for (auto const& [invasionId, definition] : sLivingWorldDataMgr.GetDefinitions())
    {
        (void)invasionId;
        if (_nextMapEvaluation.find(definition.MapId) == _nextMapEvaluation.end())
        {
            ScheduleMap(definition.MapId, now, true);
        }
    }

    _updateTimerMs = _settings.CheckIntervalSeconds * MillisecondsPerSecond;
    _initialized = true;

    LOG_INFO("server.loading", "Living World Scheduler initialized for {} map(s).", _nextMapEvaluation.size());
}

void InvasionScheduler::Update(uint32 diff)
{
    if (!_initialized || !_settings.Enabled)
    {
        return;
    }

    if (_updateTimerMs > diff)
    {
        _updateTimerMs -= diff;
        return;
    }

	_updateTimerMs =
	    std::max<uint32>(1, _settings.CheckIntervalSeconds) * MillisecondsPerSecond;

    uint64 const now = UnixTimeNow();
    EvaluateDueMaps(now);
}

void InvasionScheduler::LoadRuntimeState()
{
    if (QueryResult result = CharacterDatabase.Query(
        "SELECT `invasion_id`, `state`, `last_started_at`, `last_completed_at`, `next_eligible_at`, "
        "`active_since`, `active_until`, `times_started`, `times_completed` FROM `lw_invasion_runtime`"))
    {
        do
        {
            Field* fields = result->Fetch();

            SchedulerRuntimeRecord runtime;
            runtime.InvasionId = fields[0].Get<uint32>();
            runtime.State = static_cast<InvasionRuntimeState>(fields[1].Get<uint8>());
            runtime.LastStartedAt = fields[2].Get<uint64>();
            runtime.LastCompletedAt = fields[3].Get<uint64>();
            runtime.NextEligibleAt = fields[4].Get<uint64>();
            runtime.ActiveSince = fields[5].Get<uint64>();
            runtime.ActiveUntil = fields[6].Get<uint64>();
            runtime.TimesStarted = fields[7].Get<uint32>();
            runtime.TimesCompleted = fields[8].Get<uint32>();

            if (sLivingWorldDataMgr.GetDefinition(runtime.InvasionId))
            {
                _runtime[runtime.InvasionId] = runtime;
            }
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "[LW Scheduler] loaded {} runtime record(s).", _runtime.size());
}

void InvasionScheduler::EnsureRuntimeRows()
{
    for (auto const& [invasionId, definition] : sLivingWorldDataMgr.GetDefinitions())
    {
        (void)definition;
        if (_runtime.find(invasionId) != _runtime.end())
        {
            continue;
        }

        SchedulerRuntimeRecord runtime;
        runtime.InvasionId = invasionId;
        _runtime.emplace(invasionId, runtime);
        SaveRuntime(runtime);
    }
}

void InvasionScheduler::EvaluateDueMaps(uint64 now)
{
    for (auto& [mapId, nextEvaluation] : _nextMapEvaluation)
    {
        if (nextEvaluation > now)
        {
            continue;
        }

        EvaluateMap(mapId, now);
        ScheduleMap(mapId, now, false);
    }
}

void InvasionScheduler::EvaluateMap(uint16 mapId, uint64 now)
{
    if (_controlState != SchedulerControlState::Running)
    {
        return;
    }

    if (_settings.MaxActiveGlobal != 0 && CountActiveGlobal() >= _settings.MaxActiveGlobal)
    {
        if (_settings.Debug)
        {
            LOG_INFO("server.loading", "[LW Scheduler] map {} evaluation blocked by global active limit.", mapId);
        }
        return;
    }

    uint32 const mapLimit = GetMapLimit(mapId);
    if (mapLimit != 0 && CountActiveOnMap(mapId) >= mapLimit)
    {
        if (_settings.Debug)
        {
            LOG_INFO("server.loading", "[LW Scheduler] map {} evaluation blocked by map active limit {}.", mapId, mapLimit);
        }
        return;
    }

    std::vector<InvasionDefinition const*> candidates;
    uint64 totalWeight = 0;

    for (auto const& [invasionId, definition] : sLivingWorldDataMgr.GetDefinitions())
    {
        if (definition.MapId != mapId || !definition.AllowRandomStart)
        {
            continue;
        }

        auto runtimeIterator = _runtime.find(invasionId);
        if (runtimeIterator == _runtime.end())
        {
            continue;
        }

        SchedulerRuntimeRecord const& runtime = runtimeIterator->second;
        if (runtime.State == InvasionRuntimeState::Active || runtime.NextEligibleAt > now)
        {
            continue;
        }

        ResponseOriginDefinition const* origin = sLivingWorldDataMgr.GetResponseOrigin(definition.ResponseOriginId);
        if (!origin)
        {
            continue;
        }

        uint32 const originLimit = GetResponseOriginLimit(origin->Id, origin->MaxActiveDefault);
        if (originLimit != 0 && CountActiveForResponseOrigin(origin->Id) >= originLimit)
        {
            if (_settings.Debug)
            {
                LOG_INFO("server.loading", "[LW Scheduler] invasion {} ({}) rejected because response origin {} ({}) is at capacity.",
                    definition.Id, definition.Name, origin->Id, origin->Name);
            }
            continue;
        }

        candidates.push_back(&definition);
        totalWeight += definition.SelectionWeight;
    }

    if (candidates.empty() || totalWeight == 0)
    {
        if (_settings.Debug)
        {
            LOG_INFO("server.loading", "[LW Scheduler] no eligible invasions for map {}.", mapId);
        }
        return;
    }

    uint64 roll = urand(1, static_cast<uint32>(std::min<uint64>(totalWeight, std::numeric_limits<uint32>::max())));
    InvasionDefinition const* selected = candidates.back();

    for (InvasionDefinition const* candidate : candidates)
    {
        if (roll <= candidate->SelectionWeight)
        {
            selected = candidate;
            break;
        }

        roll -= candidate->SelectionWeight;
    }

    StartInvasion(selected->Id, now);
}

void InvasionScheduler::ScheduleMap(uint16 mapId, uint64 now, bool initial)
{
    uint32 const delay = initial
        ? RandomBetween(_settings.InitialDelayMinSeconds, _settings.InitialDelayMaxSeconds)
        : RandomBetween(_settings.NextDelayMinSeconds, _settings.NextDelayMaxSeconds);

    _nextMapEvaluation[mapId] = now + std::max<uint32>(1, delay);

    if (_settings.Debug)
    {
        LOG_INFO("server.loading", "[LW Scheduler] map {} next evaluation in {} second(s).", mapId, delay);
    }
}

bool InvasionScheduler::StartInvasion(uint32 invasionId, uint64 now)
{
    InvasionDefinition const* definition = sLivingWorldDataMgr.GetDefinition(invasionId);
    auto runtimeIterator = _runtime.find(invasionId);
    if (!definition || runtimeIterator == _runtime.end())
    {
        return false;
    }

    SchedulerRuntimeRecord& runtime = runtimeIterator->second;
    runtime.State = InvasionRuntimeState::Active;
    runtime.LastStartedAt = now;
    runtime.ActiveSince = now;
    runtime.ActiveUntil = now + definition->MaximumRuntimeSeconds;
    runtime.NextEligibleAt = 0;
    ++runtime.TimesStarted;
    SaveRuntime(runtime);

    if (!sInvasionRuntimeMgr.StartInvasion(invasionId))
    {
        LOG_ERROR("server.loading",
            "[LW Scheduler] Failed to create a runtime for invasion {} ({}); returning it to available state.",
            definition->Id, definition->Name);
        NotifyInvasionStartFailed(invasionId);
        return false;
    }

    ResponseOriginDefinition const* origin = sLivingWorldDataMgr.GetResponseOrigin(definition->ResponseOriginId);
    LOG_INFO("server.loading",
        "[LW Scheduler] Selected invasion {} ({}) on map {}; response origin {} ({}).",
        definition->Id, definition->Name, definition->MapId,
        origin ? origin->Id : 0, origin ? origin->Name : "unknown");
    return true;
}

void InvasionScheduler::NotifyInvasionCompleted(uint32 invasionId, uint64 now)
{
    InvasionDefinition const* definition = sLivingWorldDataMgr.GetDefinition(invasionId);
    auto runtimeIterator = _runtime.find(invasionId);
    if (!definition || runtimeIterator == _runtime.end())
    {
        return;
    }

    SchedulerRuntimeRecord& runtime = runtimeIterator->second;
    uint32 const cooldown = RandomBetween(definition->MinimumCooldownSeconds, definition->MaximumCooldownSeconds);

    runtime.State = InvasionRuntimeState::Cooldown;
    runtime.LastCompletedAt = now;
    runtime.NextEligibleAt = now + cooldown;
    runtime.ActiveSince = 0;
    runtime.ActiveUntil = 0;
    ++runtime.TimesCompleted;
    SaveRuntime(runtime);

    LOG_INFO("server.loading",
        "[LW Scheduler] Invasion {} ({}) completed and entered cooldown for {} second(s).",
        definition->Id, definition->Name, cooldown);
}

void InvasionScheduler::NotifyInvasionTimedOut(uint32 invasionId, uint64 now)
{
    InvasionDefinition const* definition = sLivingWorldDataMgr.GetDefinition(invasionId);
    auto runtimeIterator = _runtime.find(invasionId);
    if (!definition || runtimeIterator == _runtime.end())
    {
        return;
    }

    SchedulerRuntimeRecord& runtime = runtimeIterator->second;
    uint32 const cooldown = RandomBetween(definition->MinimumCooldownSeconds, definition->MaximumCooldownSeconds);

    runtime.State = InvasionRuntimeState::Cooldown;
    runtime.LastCompletedAt = now;
    runtime.NextEligibleAt = now + cooldown;
    runtime.ActiveSince = 0;
    runtime.ActiveUntil = 0;
    SaveRuntime(runtime);

    LOG_ERROR("server.loading",
        "[LW Scheduler] Invasion {} ({}) exceeded its maximum runtime and entered cooldown for {} second(s).",
        definition->Id, definition->Name, cooldown);
}

void InvasionScheduler::NotifyInvasionFailed(uint32 invasionId, uint64 now)
{
    InvasionDefinition const* definition = sLivingWorldDataMgr.GetDefinition(invasionId);
    auto runtimeIterator = _runtime.find(invasionId);
    if (!definition || runtimeIterator == _runtime.end())
    {
        return;
    }

    SchedulerRuntimeRecord& runtime = runtimeIterator->second;
    uint32 const cooldown = RandomBetween(
        definition->MinimumCooldownSeconds,
        definition->MaximumCooldownSeconds);

    runtime.State = InvasionRuntimeState::Cooldown;
    runtime.NextEligibleAt = now + cooldown;
    runtime.ActiveSince = 0;
    runtime.ActiveUntil = 0;
    SaveRuntime(runtime);

    LOG_INFO("server.loading",
        "[LW Scheduler] Invasion {} ({}) failed because its active force was defeated "
        "and entered cooldown for {} second(s).",
        definition->Id,
        definition->Name,
        cooldown);
}

void InvasionScheduler::NotifyInvasionStartFailed(uint32 invasionId)
{
    auto runtimeIterator = _runtime.find(invasionId);
    if (runtimeIterator == _runtime.end())
    {
        return;
    }

    SchedulerRuntimeRecord& runtime = runtimeIterator->second;
    runtime.State = InvasionRuntimeState::Available;
    runtime.ActiveSince = 0;
    runtime.ActiveUntil = 0;
    runtime.NextEligibleAt = 0;
    if (runtime.TimesStarted > 0)
    {
        --runtime.TimesStarted;
    }
    SaveRuntime(runtime);
}

bool InvasionScheduler::IsInvasionActive(uint32 invasionId) const
{
    auto iterator = _runtime.find(invasionId);
    return iterator != _runtime.end() && iterator->second.State == InvasionRuntimeState::Active;
}

std::vector<uint32> InvasionScheduler::GetActiveInvasionIds() const
{
    std::vector<uint32> active;
    for (auto const& [invasionId, runtime] : _runtime)
    {
        if (runtime.State == InvasionRuntimeState::Active)
        {
            active.push_back(invasionId);
        }
    }
    return active;
}

void InvasionScheduler::SaveRuntime(SchedulerRuntimeRecord const& runtime)
{
    CharacterDatabase.Execute(
        "REPLACE INTO `lw_invasion_runtime` "
        "(`invasion_id`, `state`, `last_started_at`, `last_completed_at`, `next_eligible_at`, `active_since`, `active_until`, `times_started`, `times_completed`) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {})",
        runtime.InvasionId,
        static_cast<uint32>(runtime.State),
        runtime.LastStartedAt,
        runtime.LastCompletedAt,
        runtime.NextEligibleAt,
        runtime.ActiveSince,
        runtime.ActiveUntil,
        runtime.TimesStarted,
        runtime.TimesCompleted);
}

uint32 InvasionScheduler::GetMapLimit(uint16 mapId) const
{
    std::string const key = "LivingWorld.Scheduler.MaxActive.Map." + std::to_string(mapId);
    return sConfigMgr->GetOption<uint32>(key, _settings.DefaultMaxActivePerMap);
}

uint32 InvasionScheduler::GetResponseOriginLimit(uint32 responseOriginId, uint32 sqlDefault) const
{
    std::string const key = "LivingWorld.Scheduler.MaxActive.ResponseOrigin." + std::to_string(responseOriginId);
    uint32 const fallback = sqlDefault == 0 ? _settings.DefaultMaxActivePerResponseOrigin : sqlDefault;
    return sConfigMgr->GetOption<uint32>(key, fallback);
}

uint32 InvasionScheduler::CountActiveGlobal() const
{
    return static_cast<uint32>(std::count_if(_runtime.begin(), _runtime.end(), [](auto const& item)
    {
        return item.second.State == InvasionRuntimeState::Active;
    }));
}

uint32 InvasionScheduler::CountActiveOnMap(uint16 mapId) const
{
    uint32 count = 0;
    for (auto const& [invasionId, runtime] : _runtime)
    {
        InvasionDefinition const* definition = sLivingWorldDataMgr.GetDefinition(invasionId);
        if (definition && definition->MapId == mapId && runtime.State == InvasionRuntimeState::Active)
        {
            ++count;
        }
    }
    return count;
}

uint32 InvasionScheduler::CountActiveForResponseOrigin(uint32 responseOriginId) const
{
    uint32 count = 0;
    for (auto const& [invasionId, runtime] : _runtime)
    {
        InvasionDefinition const* definition = sLivingWorldDataMgr.GetDefinition(invasionId);
        if (definition && definition->ResponseOriginId == responseOriginId && runtime.State == InvasionRuntimeState::Active)
        {
            ++count;
        }
    }
    return count;
}

std::string InvasionScheduler::BuildStatusReport() const
{
    std::ostringstream output;
    uint64 const now = UnixTimeNow();

    output << "Living World Scheduler\n";
    output << "--------------------------------\n";
    output << "Enabled: " << (_settings.Enabled ? "yes" : "no") << '\n';
    output << "Initialized: " << (_initialized ? "yes" : "no") << '\n';
    output << "Control state: ";
    switch (_controlState)
    {
        case SchedulerControlState::Running:
            output << "running\n";
            break;
        case SchedulerControlState::Paused:
            output << "paused\n";
            break;
        case SchedulerControlState::Draining:
            output << "draining\n";
            break;
        default:
            output << "unknown\n";
            break;
    }
    output << "Runtime records: " << _runtime.size() << '\n';
    output << "Active globally: " << CountActiveGlobal();

    if (_settings.MaxActiveGlobal != 0)
    {
        output << " / " << _settings.MaxActiveGlobal;
    }

    output << "\n\nMap evaluations:\n";

    if (_nextMapEvaluation.empty())
    {
        output << "  None scheduled.\n";
    }
    else
    {
        for (auto const& [mapId, nextEvaluation] : _nextMapEvaluation)
        {
            uint64 const remaining =
                nextEvaluation > now ? nextEvaluation - now : 0;

            output << "  Map " << mapId
                   << ": next evaluation in "
                   << remaining << " second(s)\n";
        }
    }

    output << "\nInvasions:\n";

    if (_runtime.empty())
    {
        output << "  No runtime records loaded.\n";
        return output.str();
    }

    for (auto const& [invasionId, runtime] : _runtime)
    {
        InvasionDefinition const* definition =
            sLivingWorldDataMgr.GetDefinition(invasionId);

        output << "  [" << invasionId << "] ";

        if (definition)
        {
            output << definition->Name
                   << " (map " << definition->MapId << ")";
        }
        else
        {
            output << "Unknown definition";
        }

        output << '\n';

        switch (runtime.State)
        {
			case InvasionRuntimeState::Available:
			    output << "      State: available\n";
			    break;

            case InvasionRuntimeState::Active:
            {
                output << "      State: active\n";
                if (InvasionRuntime const* activeRuntime = sInvasionRuntimeMgr.GetRuntimeForInvasion(invasionId))
                {
                    output << "      " << activeRuntime->BuildStatusLine(now) << '\n';
                    if (runtime.ActiveUntil != 0)
                    {
                        uint64 const timeoutRemaining =
                            runtime.ActiveUntil > now ? runtime.ActiveUntil - now : 0;
                        output << "      Hard timeout remaining: "
                               << timeoutRemaining << " second(s)\n";
                    }
                }
                else
                {
                    output << "      Runtime: not loaded\n";
                }
                break;
            }

            case InvasionRuntimeState::Cooldown:
            {
                uint64 const remaining =
                    runtime.NextEligibleAt > now
                        ? runtime.NextEligibleAt - now
                        : 0;

                output << "      State: cooldown\n";
                output << "      Cooldown remaining: "
                       << remaining << " second(s)\n";
                break;
            }

            default:
                output << "      State: unknown\n";
                break;
        }

        output << "      Times started: "
               << runtime.TimesStarted << '\n';

        output << "      Times completed: "
               << runtime.TimesCompleted << '\n';

        if (definition)
        {
            uint32 const originActive =
                CountActiveForResponseOrigin(
                    definition->ResponseOriginId);

            ResponseOriginDefinition const* origin =
                sLivingWorldDataMgr.GetResponseOrigin(
                    definition->ResponseOriginId);

            output << "      Response origin: ";

            if (origin)
            {
                output << origin->Name
                       << " [" << origin->Id << "]";
            }
            else
            {
                output << "unknown ["
                       << definition->ResponseOriginId << "]";
            }

            output << ", active invasions: "
                   << originActive << '\n';
        }
    }

    return output.str();
}

}
