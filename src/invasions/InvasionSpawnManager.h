#ifndef MOD_LIVING_WORLD_SPAWN_MANAGER_H
#define MOD_LIVING_WORLD_SPAWN_MANAGER_H

#include "Define.h"
#include "IEntityProvider.h"

#include <memory>
#include <unordered_map>

namespace lw
{
class InvasionSpawnManager
{
public:
    static InvasionSpawnManager& Instance();

    void Reset();
    bool SpawnGroup(uint64 runtimeId, uint32 spawnGroupId, uint64* runtimeGroupId = nullptr);

    bool StartGarrison(
        uint64 runtimeId,
        uint32 spawnGroupId,
        uint32 quietPeriodSeconds,
        uint32 refillBatchSize,
        uint32 refillIntervalSeconds);

    void UpdateGarrisons(uint64 runtimeId, uint64 nowSeconds);
    void CleanupRuntime(uint64 runtimeId);

private:
    struct ActiveGarrison
    {
        uint64 RuntimeId = 0;
        uint64 RuntimeGroupId = 0;
        uint32 SpawnGroupId = 0;
        uint32 QuietPeriodSeconds = 30;
        uint32 RefillBatchSize = 5;
        uint32 RefillIntervalSeconds = 10;
        uint64 QuietSince = 0;
        uint64 NextRefillAt = 0;
    };

    InvasionSpawnManager();

    IEntityProvider* GetProvider(uint8 entityType);
    void RegisterProvider(std::unique_ptr<IEntityProvider> provider);

    bool IsGarrisonQuiet(ActiveGarrison const& garrison) const;
    bool RefillGarrison(ActiveGarrison& garrison, uint64 nowSeconds);

    std::unordered_map<uint8, std::unique_ptr<IEntityProvider>> _providers;
    std::unordered_map<uint64, ActiveGarrison> _activeGarrisons;
};
}

#define sInvasionSpawnMgr lw::InvasionSpawnManager::Instance()

#endif
