#ifndef MOD_LIVING_WORLD_CREATURE_ABILITY_MANAGER_H
#define MOD_LIVING_WORLD_CREATURE_ABILITY_MANAGER_H

#include "Define.h"
#include "ObjectGuid.h"

#include <unordered_map>
#include <vector>

class Unit;
class Creature;

namespace lw
{
enum class CreatureAbilityTargetType : uint8
{
    Self = 0,
    FriendlyLowestHealth = 1,
    FriendlyRandom = 2,
    HostileVictim = 3
};

struct CreatureAbilityDefinition
{
    uint32 Id = 0;
    uint32 LwTemplateId = 0;
    uint32 SpellId = 0;
    CreatureAbilityTargetType TargetType = CreatureAbilityTargetType::Self;
    uint8 Priority = 0;
    float HealthThresholdPct = 100.0f;
    uint32 CooldownMs = 0;
    float Range = 0.0f;
    bool RequireCombat = true;
    bool Enabled = false;
};

class CreatureAbilityManager
{
public:
    static CreatureAbilityManager& Instance();

    void Reset();
    void Load();
    void Update(uint32 diff);
    void CancelRuntime(uint64 runtimeId);

    [[nodiscard]] uint32 GetAbilityCount() const;

private:
    CreatureAbilityManager() = default;

    struct CooldownKey
    {
        ObjectGuid CasterGuid;
        uint32 AbilityId = 0;

        bool operator==(CooldownKey const& other) const
        {
            return CasterGuid == other.CasterGuid && AbilityId == other.AbilityId;
        }
    };

    struct CooldownKeyHash
    {
        std::size_t operator()(CooldownKey const& key) const;
    };

    bool TryExecute(uint64 runtimeId, struct RuntimeEntity const& entity, CreatureAbilityDefinition const& ability);
    Unit* SelectTarget(uint64 runtimeId, Creature* caster, CreatureAbilityDefinition const& ability) const;

    std::unordered_map<uint32, std::vector<CreatureAbilityDefinition>> _abilitiesByTemplate;
    std::unordered_map<CooldownKey, uint32, CooldownKeyHash> _cooldowns;
    uint32 _updateTimerMs = 0;
};
}

#define sCreatureAbilityMgr lw::CreatureAbilityManager::Instance()

#endif
