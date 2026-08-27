#include "RuntimeSignalManager.h"

#include "LivingWorld.h"
#include "Log.h"

#include <algorithm>
#include <ctime>
#include <sstream>

namespace lw
{
namespace
{
uint64 UnixTimeNow()
{
    return static_cast<uint64>(std::time(nullptr));
}
}

RuntimeSignalManager& RuntimeSignalManager::Instance()
{
    static RuntimeSignalManager instance;
    return instance;
}

void RuntimeSignalManager::Reset()
{
    _signalsByRuntime.clear();
}

bool RuntimeSignalManager::EmitSignal(uint64 runtimeId, uint32 signalId)
{
    RuntimeSignalDefinition const* definition = sLivingWorldDataMgr.GetRuntimeSignal(signalId);
    if (!definition)
    {
        LOG_ERROR("server.loading",
            "[LW Signal] Runtime #{} attempted to emit missing or disabled signal {}.",
            runtimeId, signalId);
        return false;
    }

    auto& runtimeSignals = _signalsByRuntime[runtimeId];
    auto existing = runtimeSignals.find(signalId);
    if (existing != runtimeSignals.end())
    {
        // Signals are intentionally idempotent within a runtime.
        return true;
    }

    RuntimeSignal signal;
    signal.SignalId = signalId;
    signal.EmittedAt = UnixTimeNow();
    runtimeSignals.emplace(signalId, signal);

    LOG_INFO("server.loading",
        "[LW Signal] Runtime #{} emitted signal {} ({}).",
        runtimeId, signalId, definition->Name);

    return true;
}

bool RuntimeSignalManager::HasSignal(uint64 runtimeId, uint32 signalId) const
{
    auto runtimeItr = _signalsByRuntime.find(runtimeId);
    if (runtimeItr == _signalsByRuntime.end())
    {
        return false;
    }

    return runtimeItr->second.find(signalId) != runtimeItr->second.end();
}

std::vector<RuntimeSignal> RuntimeSignalManager::GetSignals(uint64 runtimeId) const
{
    std::vector<RuntimeSignal> result;

    auto runtimeItr = _signalsByRuntime.find(runtimeId);
    if (runtimeItr == _signalsByRuntime.end())
    {
        return result;
    }

    result.reserve(runtimeItr->second.size());
    for (auto const& [signalId, signal] : runtimeItr->second)
    {
        (void)signalId;
        result.push_back(signal);
    }

    std::sort(result.begin(), result.end(), [](RuntimeSignal const& left, RuntimeSignal const& right)
    {
        if (left.EmittedAt != right.EmittedAt)
        {
            return left.EmittedAt < right.EmittedAt;
        }

        return left.SignalId < right.SignalId;
    });

    return result;
}

void RuntimeSignalManager::ClearRuntime(uint64 runtimeId)
{
    _signalsByRuntime.erase(runtimeId);
}

std::string RuntimeSignalManager::BuildStatusReport() const
{
    std::ostringstream output;
    output << "Runtime signals:\n";

    if (_signalsByRuntime.empty())
    {
        output << "  None.\n";
        return output.str();
    }

    std::vector<uint64> runtimeIds;
    runtimeIds.reserve(_signalsByRuntime.size());
    for (auto const& [runtimeId, signals] : _signalsByRuntime)
    {
        (void)signals;
        runtimeIds.push_back(runtimeId);
    }
    std::sort(runtimeIds.begin(), runtimeIds.end());

    for (uint64 runtimeId : runtimeIds)
    {
        output << "  Runtime #" << runtimeId << ":\n";

        for (RuntimeSignal const& signal : GetSignals(runtimeId))
        {
            RuntimeSignalDefinition const* definition = sLivingWorldDataMgr.GetRuntimeSignal(signal.SignalId);
            output << "    [" << signal.SignalId << "] ";

            if (definition)
            {
                output << definition->Name;
            }
            else
            {
                output << "Unknown Signal";
            }

            output << " (emitted at " << signal.EmittedAt << ")\n";
        }
    }

    return output.str();
}
}
