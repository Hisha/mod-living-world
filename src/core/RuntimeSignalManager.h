#ifndef MOD_LIVING_WORLD_RUNTIME_SIGNAL_MANAGER_H
#define MOD_LIVING_WORLD_RUNTIME_SIGNAL_MANAGER_H

#include "Define.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace lw
{
struct RuntimeSignal
{
    uint32 SignalId = 0;
    uint64 EmittedAt = 0;
};

class RuntimeSignalManager
{
public:
    static RuntimeSignalManager& Instance();

    void Reset();

    bool EmitSignal(uint64 runtimeId, uint32 signalId);
    [[nodiscard]] bool HasSignal(uint64 runtimeId, uint32 signalId) const;
    [[nodiscard]] std::vector<RuntimeSignal> GetSignals(uint64 runtimeId) const;

    void ClearRuntime(uint64 runtimeId);

    [[nodiscard]] std::string BuildStatusReport() const;

private:
    RuntimeSignalManager() = default;

    std::unordered_map<uint64, std::unordered_map<uint32, RuntimeSignal>> _signalsByRuntime;
};
}

#define sRuntimeSignalMgr lw::RuntimeSignalManager::Instance()

#endif
