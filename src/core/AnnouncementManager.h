#ifndef MOD_LIVING_WORLD_ANNOUNCEMENT_MANAGER_H
#define MOD_LIVING_WORLD_ANNOUNCEMENT_MANAGER_H

#include "Define.h"

namespace lw
{
enum class AnnouncementScope : uint8
{
    Global = 0,
    Map = 1,
    Zone = 2,
    Area = 3
};

enum class AnnouncementFaction : uint8
{
    Everyone = 0,
    Alliance = 1,
    Horde = 2
};

class AnnouncementManager
{
public:
    static AnnouncementManager& Instance();

    bool Execute(uint64 runtimeId, uint32 invasionId, uint32 announcementId,
        uint32 scopeValue, uint32 scopeId, uint32 factionValue);

private:
    AnnouncementManager() = default;
};
}

#define sAnnouncementManager lw::AnnouncementManager::Instance()

#endif