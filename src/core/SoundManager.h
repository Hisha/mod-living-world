#ifndef MOD_LIVING_WORLD_SOUND_MANAGER_H
#define MOD_LIVING_WORLD_SOUND_MANAGER_H

#include "Define.h"

namespace lw
{
enum class SoundPlaybackMode : uint8
{
    Distance = 0,
    Direct = 1
};

class SoundManager
{
public:
    static SoundManager& Instance();

    bool Execute(uint64 runtimeId, uint32 spawnGroupId, uint32 soundId,
        uint32 sourceMemberId, uint32 playbackMode);

private:
    SoundManager() = default;
};
}

#define sSoundManager lw::SoundManager::Instance()

#endif