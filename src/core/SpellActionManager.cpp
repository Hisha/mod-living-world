#include "SpellActionManager.h"

#include "Creature.h"
#include "IEntityProvider.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "RuntimeEntityGroup.h"
#include "SpellMgr.h"

namespace lw
{
SpellActionManager& SpellActionManager::Instance()
{
    static SpellActionManager instance;
    return instance;
}

bool SpellActionManager::ExecuteSelfCast(uint64 runtimeId, uint32 spawnGroupId, uint32 spellId,
    uint32 casterMemberId, uint32 targetMode)
{
    if (spellId == 0)
    {
        LOG_ERROR("server.loading",
            "[LW Spell] Runtime #{} cannot execute spell action with spell id 0.",
            runtimeId);
        return false;
    }

    if (targetMode != static_cast<uint32>(SpellActionTargetMode::Self))
    {
        LOG_ERROR("server.loading",
            "[LW Spell] Runtime #{} spell {} uses unsupported target mode {}. "
            "Spell Actions v1 supports Self (0) only.",
            runtimeId, spellId, targetMode);
        return false;
    }

    if (!sSpellMgr->GetSpellInfo(spellId))
    {
        LOG_ERROR("server.loading",
            "[LW Spell] Runtime #{} cannot execute unknown spell id {}.",
            runtimeId, spellId);
        return false;
    }

    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.FindLatestGroup(runtimeId, spawnGroupId);
    if (!group)
    {
        LOG_ERROR("server.loading",
            "[LW Spell] Runtime #{} cannot cast spell {} because spawn group {} has no runtime entity group.",
            runtimeId, spellId, spawnGroupId);
        return false;
    }

    RuntimeEntity const* casterEntity = nullptr;

    for (RuntimeEntity const& entity : group->Entities)
    {
        if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
        {
            continue;
        }

        if (casterMemberId != 0 && entity.MemberId != casterMemberId)
        {
            continue;
        }

        Map* map = sMapMgr->FindMap(entity.MapId, 0);
        if (!map)
        {
            continue;
        }

        Creature* creature = map->GetCreature(entity.Guid);
        if (!creature || !creature->IsAlive())
        {
            continue;
        }

        casterEntity = &entity;
        break;
    }

    if (!casterEntity)
    {
        LOG_ERROR("server.loading",
            "[LW Spell] Runtime #{} spell {} could not find an available creature caster "
            "in runtime entity group #{} (member {}).",
            runtimeId, spellId, group->Id, casterMemberId);
        return false;
    }

    Map* map = sMapMgr->FindMap(casterEntity->MapId, 0);
    if (!map)
    {
        return false;
    }

    Creature* caster = map->GetCreature(casterEntity->Guid);
    if (!caster)
    {
        return false;
    }

    // Explicit invasion-scripted cast. Native CreatureAI/SmartAI remains
    // responsible for normal combat spells and automatic self-buffs.
    caster->CastSpell(caster, spellId, true);

    LOG_INFO("server.loading",
        "[LW Spell] Runtime #{} runtime entity group #{} member {} cast spell {} on self.",
        runtimeId,
        group->Id,
        casterEntity->MemberId,
        spellId);

    return true;
}
}
