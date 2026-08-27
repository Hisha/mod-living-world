#include "CreatureAbilityManager.h"

#include "Creature.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "IEntityProvider.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "QueryResult.h"
#include "Random.h"
#include "RuntimeEntityGroup.h"
#include "SpellMgr.h"

#include <algorithm>
#include <limits>

namespace lw
{
namespace
{
constexpr uint32 AbilityUpdateIntervalMs = 500;

float HealthPct(Unit const* unit)
{
    return unit && unit->GetMaxHealth() != 0
        ? (100.0f * static_cast<float>(unit->GetHealth()) / static_cast<float>(unit->GetMaxHealth()))
        : 0.0f;
}
}

CreatureAbilityManager& CreatureAbilityManager::Instance()
{
    static CreatureAbilityManager instance;
    return instance;
}

std::size_t CreatureAbilityManager::CooldownKeyHash::operator()(CooldownKey const& key) const
{
    return std::hash<uint64>{}(key.CasterGuid.GetRawValue()) ^ (std::hash<uint32>{}(key.AbilityId) << 1U);
}

void CreatureAbilityManager::Reset()
{
    _abilitiesByTemplate.clear();
    _cooldowns.clear();
    _updateTimerMs = 0;
}

void CreatureAbilityManager::Load()
{
    _abilitiesByTemplate.clear();
    _cooldowns.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `lw_template_id`, `spell_id`, `target_type`, `priority`, "
        "`health_threshold_pct`, `cooldown_ms`, `range_yards`, `require_combat`, `enabled` "
        "FROM `lw_creature_ability` WHERE `enabled` = 1 ORDER BY `lw_template_id`, `priority` DESC, `id`");

    if (!result)
    {
        LOG_INFO("server.loading", "[LW Ability] Loaded 0 creature abilities.");
        return;
    }

    uint32 loaded = 0;
    do
    {
        Field* fields = result->Fetch();
        CreatureAbilityDefinition ability;
        ability.Id = fields[0].Get<uint32>();
        ability.LwTemplateId = fields[1].Get<uint32>();
        ability.SpellId = fields[2].Get<uint32>();
        uint8 const targetType = fields[3].Get<uint8>();
        ability.Priority = fields[4].Get<uint8>();
        ability.HealthThresholdPct = fields[5].Get<float>();
        ability.CooldownMs = fields[6].Get<uint32>();
        ability.Range = fields[7].Get<float>();
        ability.RequireCombat = fields[8].Get<bool>();
        ability.Enabled = fields[9].Get<bool>();

        if (targetType > static_cast<uint8>(CreatureAbilityTargetType::HostileVictim))
        {
            LOG_ERROR("server.loading", "[LW Ability] Ability {} has invalid target_type {} and was skipped.", ability.Id, targetType);
            continue;
        }

        if (!sSpellMgr->GetSpellInfo(ability.SpellId))
        {
            LOG_ERROR("server.loading", "[LW Ability] Ability {} references unknown spell {} and was skipped.", ability.Id, ability.SpellId);
            continue;
        }

        ability.TargetType = static_cast<CreatureAbilityTargetType>(targetType);
        _abilitiesByTemplate[ability.LwTemplateId].push_back(ability);
        ++loaded;
    } while (result->NextRow());

    LOG_INFO("server.loading", "[LW Ability] Loaded {} creature abilities for {} LW creature template(s).", loaded, _abilitiesByTemplate.size());
}

uint32 CreatureAbilityManager::GetAbilityCount() const
{
    uint32 count = 0;
    for (auto const& [templateId, abilities] : _abilitiesByTemplate)
    {
        (void)templateId;
        count += static_cast<uint32>(abilities.size());
    }
    return count;
}

void CreatureAbilityManager::Update(uint32 diff)
{
    for (auto itr = _cooldowns.begin(); itr != _cooldowns.end();)
    {
        if (itr->second <= diff)
        {
            itr = _cooldowns.erase(itr);
        }
        else
        {
            itr->second -= diff;
            ++itr;
        }
    }

    if (_updateTimerMs > diff)
    {
        _updateTimerMs -= diff;
        return;
    }
    _updateTimerMs = AbilityUpdateIntervalMs;

    // RuntimeEntityGroupManager is the authoritative owner of temporary LW entities.
    // Process each active runtime group and allow at most one successful ability cast
    // per creature on each manager tick.
    std::vector<uint64> const groupIds = sRuntimeEntityGroupMgr.GetAllGroupIds();
    for (uint64 groupId : groupIds)
    {
        RuntimeEntityGroup const* group = sRuntimeEntityGroupMgr.GetGroup(groupId);
        if (!group || group->State != RuntimeEntityGroupState::Active)
        {
            continue;
        }

        for (RuntimeEntity const& entity : group->Entities)
        {
            if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature) || entity.LwTemplateId == 0)
            {
                continue;
            }

            auto const abilityItr = _abilitiesByTemplate.find(entity.LwTemplateId);
            if (abilityItr == _abilitiesByTemplate.end())
            {
                continue;
            }

            for (CreatureAbilityDefinition const& ability : abilityItr->second)
            {
                if (TryExecute(group->RuntimeId, entity, ability))
                {
                    break;
                }
            }
        }
    }
}

bool CreatureAbilityManager::TryExecute(uint64 runtimeId, RuntimeEntity const& entity, CreatureAbilityDefinition const& ability)
{
    CooldownKey const cooldownKey{ entity.Guid, ability.Id };
    if (_cooldowns.find(cooldownKey) != _cooldowns.end())
    {
        return false;
    }

    Map* map = sMapMgr->FindMap(entity.MapId, 0);
    if (!map)
    {
        return false;
    }

    Creature* caster = map->GetCreature(entity.Guid);
    if (!caster || !caster->IsAlive() || caster->IsNonMeleeSpellCast(false))
    {
        return false;
    }

    if (ability.RequireCombat && !caster->IsInCombat())
    {
        return false;
    }

    Unit* target = SelectTarget(runtimeId, caster, ability);
    if (!target)
    {
        return false;
    }

    caster->CastSpell(target, ability.SpellId, false);
    _cooldowns[cooldownKey] = ability.CooldownMs;

    LOG_INFO("server.loading",
        "[LW Ability] Runtime #{} creature {} cast spell {} (ability {}) on {}.",
        runtimeId, entity.Entry, ability.SpellId, ability.Id, target->GetGUID().ToString());
    return true;
}

Unit* CreatureAbilityManager::SelectTarget(uint64 runtimeId, Creature* caster, CreatureAbilityDefinition const& ability) const
{
    if (ability.TargetType == CreatureAbilityTargetType::Self)
    {
        if (HealthPct(caster) > ability.HealthThresholdPct)
        {
            return nullptr;
        }
        return caster;
    }

    if (ability.TargetType == CreatureAbilityTargetType::HostileVictim)
    {
        Unit* victim = caster->GetVictim();
        if (!victim || !victim->IsAlive())
        {
            return nullptr;
        }
        if (ability.Range > 0.0f && !caster->IsWithinDistInMap(victim, ability.Range))
        {
            return nullptr;
        }
        return victim;
    }

    std::vector<Creature*> candidates;
    Creature* lowest = nullptr;
    float lowestHealth = std::numeric_limits<float>::max();

    for (uint64 groupId : sRuntimeEntityGroupMgr.GetGroupsForRuntime(runtimeId))
    {
        RuntimeEntityGroup const* group = sRuntimeEntityGroupMgr.GetGroup(groupId);
        if (!group || group->State != RuntimeEntityGroupState::Active)
        {
            continue;
        }

        for (RuntimeEntity const& entity : group->Entities)
        {
            if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
            {
                continue;
            }

            Map* map = sMapMgr->FindMap(entity.MapId, 0);
            Creature* candidate = map ? map->GetCreature(entity.Guid) : nullptr;
            if (!candidate || !candidate->IsAlive() || candidate->GetMap() != caster->GetMap())
            {
                continue;
            }

            if (ability.Range > 0.0f && !caster->IsWithinDistInMap(candidate, ability.Range))
            {
                continue;
            }

            float const pct = HealthPct(candidate);
            if (pct > ability.HealthThresholdPct)
            {
                continue;
            }

            candidates.push_back(candidate);
            if (pct < lowestHealth)
            {
                lowestHealth = pct;
                lowest = candidate;
            }
        }
    }

    if (ability.TargetType == CreatureAbilityTargetType::FriendlyLowestHealth)
    {
        return lowest;
    }

    if (ability.TargetType == CreatureAbilityTargetType::FriendlyRandom && !candidates.empty())
    {
        return candidates[urand(0, static_cast<uint32>(candidates.size() - 1))];
    }

    return nullptr;
}

void CreatureAbilityManager::CancelRuntime(uint64 runtimeId)
{
    for (uint64 groupId : sRuntimeEntityGroupMgr.GetGroupsForRuntime(runtimeId))
    {
        RuntimeEntityGroup const* group = sRuntimeEntityGroupMgr.GetGroup(groupId);
        if (!group)
        {
            continue;
        }

        for (RuntimeEntity const& entity : group->Entities)
        {
            for (auto itr = _cooldowns.begin(); itr != _cooldowns.end();)
            {
                if (itr->first.CasterGuid == entity.Guid)
                {
                    itr = _cooldowns.erase(itr);
                }
                else
                {
                    ++itr;
                }
            }
        }
    }
}
}
