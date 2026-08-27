#include "AnnouncementManager.h"

#include "Chat.h"
#include "LivingWorld.h"
#include "Log.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

namespace lw
{
namespace
{
char const* ScopeName(AnnouncementScope scope)
{
    switch (scope)
    {
        case AnnouncementScope::Global: return "Global";
        case AnnouncementScope::Map:    return "Map";
        case AnnouncementScope::Zone:   return "Zone";
        case AnnouncementScope::Area:   return "Area";
        default:                        return "Unknown";
    }
}

char const* FactionName(AnnouncementFaction faction)
{
    switch (faction)
    {
        case AnnouncementFaction::Everyone: return "Everyone";
        case AnnouncementFaction::Alliance: return "Alliance";
        case AnnouncementFaction::Horde:    return "Horde";
        default:                            return "Unknown";
    }
}

bool MatchesFaction(Player const* player, AnnouncementFaction faction)
{
    if (!player)
    {
        return false;
    }

    switch (faction)
    {
        case AnnouncementFaction::Everyone:
            return true;
		case AnnouncementFaction::Alliance:
			return player->GetTeamId() == TEAM_ALLIANCE;
		case AnnouncementFaction::Horde:
			return player->GetTeamId() == TEAM_HORDE;
        default:
            return false;
    }
}
}

AnnouncementManager& AnnouncementManager::Instance()
{
    static AnnouncementManager instance;
    return instance;
}

bool AnnouncementManager::Execute(uint64 runtimeId, uint32 invasionId, uint32 announcementId,
    uint32 scopeValue, uint32 scopeId, uint32 factionValue)
{
    AnnouncementDefinition const* announcement = sLivingWorldDataMgr.GetAnnouncement(announcementId);
    if (!announcement)
    {
        LOG_ERROR("server.loading",
            "[LW Announcement] Runtime #{} could not execute missing/disabled announcement {}.",
            runtimeId, announcementId);
        return false;
    }

    InvasionDefinition const* invasion = sLivingWorldDataMgr.GetDefinition(invasionId);
    if (!invasion)
    {
        LOG_ERROR("server.loading",
            "[LW Announcement] Runtime #{} could not resolve invasion {} for announcement {}.",
            runtimeId, invasionId, announcementId);
        return false;
    }

    if (scopeValue > static_cast<uint32>(AnnouncementScope::Area))
    {
        LOG_ERROR("server.loading",
            "[LW Announcement] Runtime #{} announcement {} uses unsupported scope {}.",
            runtimeId, announcementId, scopeValue);
        return false;
    }

    if (factionValue > static_cast<uint32>(AnnouncementFaction::Horde))
    {
        LOG_ERROR("server.loading",
            "[LW Announcement] Runtime #{} announcement {} uses unsupported faction filter {}.",
            runtimeId, announcementId, factionValue);
        return false;
    }

    AnnouncementScope const scope = static_cast<AnnouncementScope>(scopeValue);
    AnnouncementFaction const faction = static_cast<AnnouncementFaction>(factionValue);

    uint32 resolvedScopeId = scopeId;
    switch (scope)
    {
        case AnnouncementScope::Global:
            resolvedScopeId = 0;
            break;

        case AnnouncementScope::Map:
            if (resolvedScopeId == 0)
            {
                resolvedScopeId = invasion->MapId;
            }
            break;

        case AnnouncementScope::Zone:
            if (resolvedScopeId == 0)
            {
                resolvedScopeId = invasion->ZoneId;
            }
            break;

        case AnnouncementScope::Area:
            // An invasion currently has map_id and zone_id, but no canonical area_id.
            // Area scope therefore requires an explicit parameter2 value.
            if (resolvedScopeId == 0)
            {
                LOG_ERROR("server.loading",
                    "[LW Announcement] Runtime #{} announcement {} uses Area scope but no explicit area id.",
                    runtimeId, announcementId);
                return false;
            }
            break;
    }

    uint32 recipients = 0;
    WorldSessionMgr::SessionMap const& sessions = sWorldSessionMgr->GetAllSessions();

    for (auto const& [accountId, session] : sessions)
    {
        (void)accountId;

        if (!session)
        {
            continue;
        }

        Player* player = session->GetPlayer();
        if (!player || !player->IsInWorld() || !MatchesFaction(player, faction))
        {
            continue;
        }

        bool matchesScope = false;
        switch (scope)
        {
            case AnnouncementScope::Global:
                matchesScope = true;
                break;

            case AnnouncementScope::Map:
                matchesScope = player->GetMapId() == resolvedScopeId;
                break;

            case AnnouncementScope::Zone:
            {
                uint32 zoneId = 0;
                uint32 areaId = 0;
                player->GetZoneAndAreaId(zoneId, areaId);
                matchesScope = zoneId == resolvedScopeId;
                break;
            }

            case AnnouncementScope::Area:
            {
                uint32 zoneId = 0;
                uint32 areaId = 0;
                player->GetZoneAndAreaId(zoneId, areaId);
                matchesScope = areaId == resolvedScopeId;
                break;
            }
        }

        if (!matchesScope)
        {
            continue;
        }

		WorldPacket data;
		ChatHandler::BuildChatPacket(
		    data,
		    CHAT_MSG_SYSTEM,
		    LANG_UNIVERSAL,
		    nullptr,
		    nullptr,
		    announcement->Text);

		player->SendDirectMessage(&data);
		++recipients;
    }

    LOG_INFO("server.loading",
        "[LW Announcement] Runtime #{} executed announcement {} ({}) scope {} [{}], faction {}, recipients {}.",
        runtimeId,
        announcement->Id,
        announcement->Name,
        ScopeName(scope),
        resolvedScopeId,
        FactionName(faction),
        recipients);

    return true;
}
}