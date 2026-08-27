#ifndef MOD_LIVING_WORLD_SPELL_ACTION_MANAGER_H
#define MOD_LIVING_WORLD_SPELL_ACTION_MANAGER_H

#include "Define.h"

namespace lw
{
enum class SpellActionTargetMode : uint8
{
    Self = 0
};

class SpellActionManager
{
public:
    static SpellActionManager& Instance();

    bool ExecuteSelfCast(uint64 runtimeId, uint32 spawnGroupId, uint32 spellId,
        uint32 casterMemberId, uint32 targetMode);

private:
    SpellActionManager() = default;
};
}

#define sSpellActionManager lw::SpellActionManager::Instance()

#endif
