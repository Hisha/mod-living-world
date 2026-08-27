#include "DialogueManager.h"

#include "Creature.h"
#include "IEntityProvider.h"
#include "LivingWorld.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "RuntimeEntityGroup.h"
#include "SharedDefines.h"

namespace lw
{
DialogueManager& DialogueManager::Instance()
{
    static DialogueManager instance;
    return instance;
}

bool DialogueManager::Execute(uint64 runtimeId, uint32 spawnGroupId, uint32 dialogueId, uint32 speakerMemberId)
{
    DialogueDefinition const* dialogue = sLivingWorldDataMgr.GetDialogue(dialogueId);
    if (!dialogue)
    {
        LOG_ERROR("server.loading",
            "[LW Dialogue] Runtime #{} cannot execute missing or disabled dialogue {}.",
            runtimeId, dialogueId);
        return false;
    }

    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.FindLatestGroup(runtimeId, spawnGroupId);
    if (!group)
    {
        LOG_ERROR("server.loading",
            "[LW Dialogue] Runtime #{} cannot execute dialogue {} because spawn group {} has no runtime entity group.",
            runtimeId, dialogueId, spawnGroupId);
        return false;
    }

    RuntimeEntity const* speakerEntity = nullptr;
    for (RuntimeEntity const& entity : group->Entities)
    {
        if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
            continue;

        if (speakerMemberId != 0 && entity.MemberId != speakerMemberId)
            continue;

        Map* map = sMapMgr->FindMap(entity.MapId, 0);
        if (!map)
            continue;

        Creature* creature = map->GetCreature(entity.Guid);
        if (!creature || !creature->IsAlive())
            continue;

        speakerEntity = &entity;
        break;
    }

    if (!speakerEntity)
    {
        LOG_ERROR("server.loading",
            "[LW Dialogue] Runtime #{} dialogue {} could not find an available creature speaker in runtime entity group #{} (member {}).",
            runtimeId, dialogueId, group->Id, speakerMemberId);
        return false;
    }

    Map* map = sMapMgr->FindMap(speakerEntity->MapId, 0);
    if (!map)
        return false;

    Creature* speaker = map->GetCreature(speakerEntity->Guid);
    if (!speaker)
        return false;

    Language const language = static_cast<Language>(dialogue->Language);

    switch (dialogue->ChatType)
    {
        case 0:
            speaker->Say(dialogue->Text, language);
            break;
        case 1:
            speaker->Yell(dialogue->Text, language);
            break;
        default:
            LOG_ERROR("server.loading",
                "[LW Dialogue] Dialogue {} ({}) has unsupported chat type {}.",
                dialogue->Id, dialogue->Name, dialogue->ChatType);
            return false;
    }

    LOG_INFO("server.loading",
        "[LW Dialogue] Runtime #{} runtime entity group #{} member {} executed {} dialogue {} ({}).",
        runtimeId,
        group->Id,
        speakerEntity->MemberId,
        dialogue->ChatType == 0 ? "Say" : "Yell",
        dialogue->Id,
        dialogue->Name);

    return true;
}
}
