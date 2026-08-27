#ifndef MOD_LIVING_WORLD_GROUP_DEFEAT_WATCHER_H
#define MOD_LIVING_WORLD_GROUP_DEFEAT_WATCHER_H

#include "Define.h"

#include <unordered_map>
#include <vector>

namespace lw
{
struct GroupDefeatWatch
{
    uint64 RuntimeId = 0;
    uint32 SignalId = 0;
    bool RequireAll = true;
    bool Triggered = false;
    std::vector<uint32> SpawnGroupIds;
};

class GroupDefeatWatcher
{
public:
    static GroupDefeatWatcher& Instance();

    void Reset();
    void Update(uint32 diff);

    bool RegisterWatch(uint64 runtimeId, uint32 spawnGroupId, uint32 signalId, bool requireAll);
    void CancelRuntime(uint64 runtimeId);

private:
    bool IsSpawnGroupDefeated(uint64 runtimeId, uint32 spawnGroupId) const;

    std::unordered_map<uint64, GroupDefeatWatch> _watches;
    uint32 _updateTimerMs = 0;
};
}

#define sGroupDefeatWatcher lw::GroupDefeatWatcher::Instance()

#endif
