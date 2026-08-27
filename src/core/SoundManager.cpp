#include "SoundManager.h"

#include "Creature.h"
#include "IEntityProvider.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "RuntimeEntityGroup.h"

namespace lw
{
SoundManager& SoundManager::Instance()
{
    static SoundManager instance;
    return instance;
}

bool SoundManager::Execute(uint64 runtimeId, uint32 spawnGroupId, uint32 soundId,
    uint32 sourceMemberId, uint32 playbackMode)
{
    if (soundId == 0)
    {
        LOG_ERROR("server.loading",
            "[LW Sound] Runtime #{} cannot execute sound action with sound id 0.",
            runtimeId);
        return false;
    }

    if (playbackMode > static_cast<uint32>(SoundPlaybackMode::Direct))
    {
        LOG_ERROR("server.loading",
            "[LW Sound] Runtime #{} sound {} uses unsupported playback mode {}.",
            runtimeId, soundId, playbackMode);
        return false;
    }

    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.FindLatestGroup(runtimeId, spawnGroupId);
    if (!group)
    {
        LOG_ERROR("server.loading",
            "[LW Sound] Runtime #{} cannot play sound {} because spawn group {} has no runtime entity group.",
            runtimeId, soundId, spawnGroupId);
        return false;
    }

    RuntimeEntity const* sourceEntity = nullptr;

    for (RuntimeEntity const& entity : group->Entities)
    {
        if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
        {
            continue;
        }

        if (sourceMemberId != 0 && entity.MemberId != sourceMemberId)
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

        sourceEntity = &entity;
        break;
    }

    if (!sourceEntity)
    {
        LOG_ERROR("server.loading",
            "[LW Sound] Runtime #{} sound {} could not find an available creature source "
            "in runtime entity group #{} (member {}).",
            runtimeId, soundId, group->Id, sourceMemberId);
        return false;
    }

    Map* map = sMapMgr->FindMap(sourceEntity->MapId, 0);
    if (!map)
    {
        return false;
    }

    Creature* source = map->GetCreature(sourceEntity->Guid);
    if (!source)
    {
        return false;
    }

    SoundPlaybackMode const mode = static_cast<SoundPlaybackMode>(playbackMode);

    switch (mode)
    {
        case SoundPlaybackMode::Distance:
            source->PlayDistanceSound(soundId);
            break;

        case SoundPlaybackMode::Direct:
            source->PlayDirectSound(soundId);
            break;
    }

    LOG_INFO("server.loading",
        "[LW Sound] Runtime #{} runtime entity group #{} member {} played sound {} using {} mode.",
        runtimeId,
        group->Id,
        sourceEntity->MemberId,
        soundId,
        mode == SoundPlaybackMode::Distance ? "Distance" : "Direct");

    return true;
}
}
