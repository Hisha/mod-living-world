#ifndef MOD_LIVING_WORLD_ASSAULT_MANAGER_H
#define MOD_LIVING_WORLD_ASSAULT_MANAGER_H

#include "Define.h"
#include "ObjectGuid.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

class Creature;

namespace lw
{
struct RuntimeEntity;
struct TemporaryNpcCombatOverride
{
    ObjectGuid Guid;
    uint32 MapId = 0;
    bool WasImmuneToNpc = false;
    uint32 OriginalFaction = 0;
    uint32 TemporaryFaction = 0;
    bool FactionChanged = false;

    bool CombatNormalized = false;
    uint8 OriginalLevel = 0;
    uint32 OriginalMaxHealth = 0;
    float OriginalBaseMinDamage = 0.0f;
    float OriginalBaseMaxDamage = 0.0f;
    float OriginalOffMinDamage = 0.0f;
    float OriginalOffMaxDamage = 0.0f;
    float OriginalRangedMinDamage = 0.0f;
    float OriginalRangedMaxDamage = 0.0f;

    std::unordered_set<uint64> RuntimeIds;
};

struct AssaultWanderState
{
    ObjectGuid Guid;
    uint32 TimerMs = 0;
    bool MoveActive = false;
    uint32 MoveElapsedMs = 0;
    float DestinationX = 0.0f;
    float DestinationY = 0.0f;
    float DestinationZ = 0.0f;
};

struct ActiveAssault
{
    uint64 RuntimeId = 0;
    uint64 RuntimeGroupId = 0;
    uint32 SpawnGroupId = 0;
    float SearchRadius = 40.0f;
    uint32 ReacquireIntervalMs = 2000;
    uint32 TargetPolicy = 0;
    uint32 ReacquireTimerMs = 0;

    uint32 CenterMapId = 0;
    float CenterX = 0.0f;
    float CenterY = 0.0f;
    float CenterZ = 0.0f;
    std::vector<AssaultWanderState> WanderStates;
};

class AssaultManager
{
public:
    static AssaultManager& Instance();

    void Reset();
    void Update(uint32 diff);

    bool Start(
        uint64 runtimeId,
        uint32 spawnGroupId,
        uint32 radiusYards,
        uint32 reacquireIntervalMs,
        uint32 targetPolicy,
        uint32 assaultCenterRouteNodeId);
    void CancelRuntime(uint64 runtimeId);

private:
    bool TryAcquireTargets(ActiveAssault& assault);
    void UpdateIdleWandering(ActiveAssault& assault, uint32 diff);
    bool TryStartIdleWander(ActiveAssault& assault, RuntimeEntity const& entity, Creature* creature, AssaultWanderState& state);

    bool EnsureWorldDefenderAttackable(uint64 runtimeId, Creature* attacker, Creature* target, float searchRadius);
    void NormalizeWorldDefenderForAssault(uint64 runtimeId, Creature* attacker, Creature* target);
    TemporaryNpcCombatOverride* FindOrCreateOverride(uint64 runtimeId, Creature* target);
    void ReleaseRuntimeOverrides(uint64 runtimeId);
    void RestoreAllOverrides();
    void RestoreOverride(TemporaryNpcCombatOverride const& overrideData);

    std::unordered_map<uint64, ActiveAssault> _activeAssaults;
    std::vector<TemporaryNpcCombatOverride> _temporaryNpcOverrides;
};
}

#define sAssaultManager lw::AssaultManager::Instance()

#endif
