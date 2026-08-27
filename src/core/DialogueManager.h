#ifndef MOD_LIVING_WORLD_DIALOGUE_MANAGER_H
#define MOD_LIVING_WORLD_DIALOGUE_MANAGER_H

#include "Define.h"

namespace lw
{
class DialogueManager
{
public:
    static DialogueManager& Instance();

    // Executes a dialogue definition using a creature from the latest runtime
    // entity group created from spawnGroupId.
    // speakerMemberId = 0 selects the first available creature in the group.
    bool Execute(uint64 runtimeId, uint32 spawnGroupId, uint32 dialogueId, uint32 speakerMemberId = 0);
};
}

#define sDialogueManager lw::DialogueManager::Instance()

#endif
