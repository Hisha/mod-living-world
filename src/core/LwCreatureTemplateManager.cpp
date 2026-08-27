#include "LwCreatureTemplateManager.h"

#include "DatabaseEnv.h"
#include "Field.h"
#include "QueryResult.h"
#include "Log.h"

#include <limits>
#include <sstream>
#include <unordered_set>

namespace lw
{
namespace
{
// Keep generated entries well away from normal WotLK IDs, but DO NOT assume
// ownership of this range. Every candidate is checked against creature_template
// and the persistent LW mapping table before use.
constexpr uint32 AllocationStart = 15000000;
constexpr uint32 AllocationEnd = 16777215;

void ExecuteSql(std::string const& sql)
{
    WorldDatabase.DirectExecute(sql.c_str());
}
}

LwCreatureTemplateManager& LwCreatureTemplateManager::Instance()
{
    static LwCreatureTemplateManager instance;
    return instance;
}

bool LwCreatureTemplateManager::MaterializeStartupTemplates()
{
    _entryByLwTemplate.clear();

    LOG_INFO("server.loading",
        "[LW Template] Reconciling dynamically allocated creature templates before ObjectMgr creature-template load.");

    if (!RetireInactiveMappings())
    {
        return false;
    }

    if (!MaterializeEnabledDefinitions())
    {
        return false;
    }

    LoadMappings();

    LOG_INFO("server.loading",
        "[LW Template] Reconciliation complete. {} active LW creature template mapping(s).",
        _entryByLwTemplate.size());

    return true;
}

bool LwCreatureTemplateManager::RetireInactiveMappings()
{
    QueryResult result = WorldDatabase.Query(
        "SELECT m.`lw_template_id`, m.`allocated_entry` "
        "FROM `lw_creature_template_map` m "
        "LEFT JOIN `lw_creature_template` d ON d.`id` = m.`lw_template_id` "
        "WHERE d.`id` IS NULL OR d.`enabled` = 0");

    if (!result)
    {
        return true;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 const lwTemplateId = fields[0].Get<uint32>();
        uint32 const allocatedEntry = fields[1].Get<uint32>();

        std::ostringstream sql;
        sql << "DELETE FROM `creature_template_model` WHERE `CreatureID` = " << allocatedEntry;
        ExecuteSql(sql.str());

        sql.str("");
        sql.clear();
        sql << "DELETE FROM `creature_equip_template` WHERE `CreatureID` = " << allocatedEntry;
        ExecuteSql(sql.str());

        sql.str("");
        sql.clear();
        sql << "DELETE FROM `creature_template` WHERE `entry` = " << allocatedEntry;
        ExecuteSql(sql.str());

        sql.str("");
        sql.clear();
        sql << "UPDATE `lw_creature_template_map` SET `retired` = 1 "
            << "WHERE `lw_template_id` = " << lwTemplateId;
        ExecuteSql(sql.str());

        LOG_INFO("server.loading",
            "[LW Template] Retired logical template {} and removed generated creature entry {}. "
            "The allocated ID remains reserved and will not be reused.",
            lwTemplateId,
            allocatedEntry);
    } while (result->NextRow());

    return true;
}

bool LwCreatureTemplateManager::MaterializeEnabledDefinitions()
{
    QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `base_creature_entry` "
        "FROM `lw_creature_template` "
        "WHERE `enabled` = 1 ORDER BY `id`");

    if (!result)
    {
        LOG_INFO("server.loading", "[LW Template] No enabled derived creature templates are defined.");
        return true;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 const lwTemplateId = fields[0].Get<uint32>();
        uint32 const baseEntry = fields[1].Get<uint32>();

        if (!WorldDatabase.Query(
            ("SELECT 1 FROM `creature_template` WHERE `entry` = " + std::to_string(baseEntry) + " LIMIT 1").c_str()))
        {
            LOG_ERROR("server.loading",
                "[LW Template] Logical template {} references missing base creature entry {}; skipped.",
                lwTemplateId,
                baseEntry);
            continue;
        }

        uint32 allocatedEntry = 0;

        if (QueryResult mapping = WorldDatabase.Query(
            ("SELECT `allocated_entry` FROM `lw_creature_template_map` "
             "WHERE `lw_template_id` = " + std::to_string(lwTemplateId) + " LIMIT 1").c_str()))
        {
            allocatedEntry = mapping->Fetch()[0].Get<uint32>();
        }
        else
        {
            allocatedEntry = AllocateEntry();
            if (allocatedEntry == 0)
            {
                LOG_ERROR("server.loading",
                    "[LW Template] Could not allocate a free creature_template entry for logical template {}.",
                    lwTemplateId);
                return false;
            }

            std::ostringstream insertMap;
            insertMap
                << "INSERT INTO `lw_creature_template_map` "
                << "(`lw_template_id`,`allocated_entry`,`base_creature_entry`,`retired`) VALUES ("
                << lwTemplateId << "," << allocatedEntry << "," << baseEntry << ",0)";
            ExecuteSql(insertMap.str());

            LOG_INFO("server.loading",
                "[LW Template] Allocated creature entry {} to logical template {}.",
                allocatedEntry,
                lwTemplateId);
        }

        // The mapping owns this entry. Rebuild it from the CURRENT base row on
        // each startup so upstream AzerothCore base-template changes are inherited.
        std::ostringstream cleanup;
        cleanup << "DELETE FROM `creature_template_model` WHERE `CreatureID` = " << allocatedEntry;
        ExecuteSql(cleanup.str());

        cleanup.str("");
        cleanup.clear();
        cleanup << "DELETE FROM `creature_equip_template` WHERE `CreatureID` = " << allocatedEntry;
        ExecuteSql(cleanup.str());

        cleanup.str("");
        cleanup.clear();
        cleanup << "DELETE FROM `creature_template` WHERE `entry` = " << allocatedEntry;
        ExecuteSql(cleanup.str());

        if (!MaterializeDefinition(lwTemplateId, allocatedEntry))
        {
            LOG_ERROR("server.loading",
                "[LW Template] Failed to materialize logical template {} into creature entry {}.",
                lwTemplateId,
                allocatedEntry);
            continue;
        }

        std::ostringstream updateMap;
        updateMap
            << "UPDATE `lw_creature_template_map` m "
            << "JOIN `lw_creature_template` d ON d.`id` = m.`lw_template_id` "
            << "SET m.`base_creature_entry` = d.`base_creature_entry`, m.`retired` = 0 "
            << "WHERE m.`lw_template_id` = " << lwTemplateId;
        ExecuteSql(updateMap.str());

    } while (result->NextRow());

    return true;
}

uint32 LwCreatureTemplateManager::AllocateEntry() const
{
    // Reserved mappings are included even if their generated row is currently
    // retired, preventing automatic ID recycling.
    QueryResult used = WorldDatabase.Query(
        "SELECT `id_value` FROM ("
        " SELECT `entry` AS `id_value` FROM `creature_template` WHERE `entry` >= 15000000"
        " UNION "
        " SELECT `allocated_entry` AS `id_value` FROM `lw_creature_template_map` WHERE `allocated_entry` >= 15000000"
        ") ids ORDER BY `id_value`");

    uint32 candidate = AllocationStart;

    if (used)
    {
        do
        {
            uint32 const usedEntry = used->Fetch()[0].Get<uint32>();

            if (usedEntry < candidate)
            {
                continue;
            }

            if (usedEntry == candidate)
            {
                if (candidate == AllocationEnd)
                {
                    return 0;
                }

                ++candidate;
                continue;
            }

            // First gap.
            break;

        } while (used->NextRow());
    }

    return candidate <= AllocationEnd ? candidate : 0;
}

bool LwCreatureTemplateManager::MaterializeDefinition(uint32 lwTemplateId, uint32 allocatedEntry)
{
    // We deliberately inherit the physical/combat shell but strip world-service
    // identity and entry-specific scripting:
    //   - no gossip/vendor/quest/trainer/etc npcflags
    //   - no loot/pickpocket/skinning/gold
    //   - no SmartAI/ScriptName inheritance
    //   - no permanent movement path
    //
    // We DO inherit:
    //   - creature_template_model
    //   - creature_equip_template
    //
    // Level remains a spawn-time LW concern via level_override.
    std::ostringstream sql;
    sql <<
        "INSERT INTO `creature_template` ("
        "`entry`,`difficulty_entry_1`,`difficulty_entry_2`,`difficulty_entry_3`,"
        "`KillCredit1`,`KillCredit2`,`name`,`subname`,`IconName`,`gossip_menu_id`,"
        "`minlevel`,`maxlevel`,`exp`,`faction`,`npcflag`,`speed_walk`,`speed_run`,"
        "`speed_swim`,`speed_flight`,`detection_range`,`rank`,`dmgschool`,"
        "`DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,"
        "`unit_class`,`unit_flags`,`unit_flags2`,`dynamicflags`,`family`,`type`,`type_flags`,"
        "`lootid`,`pickpocketloot`,`skinloot`,`PetSpellDataId`,`VehicleId`,`mingold`,`maxgold`,"
        "`AIName`,`MovementType`,`HoverHeight`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,"
        "`ExperienceModifier`,`RacialLeader`,`movementId`,`RegenHealth`,`CreatureImmunitiesId`,"
        "`flags_extra`,`ScriptName`,`VerifiedBuild`) "
        "SELECT "
        << allocatedEntry << ","
        "0,0,0,0,0,"
        "COALESCE(d.`name_override`, b.`name`),"
        "COALESCE(d.`subname_override`, b.`subname`),"
        "b.`IconName`,"
        "0,"
        "b.`minlevel`,b.`maxlevel`,b.`exp`,"
        "COALESCE(d.`faction_override`, b.`faction`),"
        "0,"
        "b.`speed_walk`,b.`speed_run`,b.`speed_swim`,b.`speed_flight`,b.`detection_range`,"
        "COALESCE(d.`rank_override`, b.`rank`),"
        "b.`dmgschool`,"
        "COALESCE(d.`damage_modifier_override`, b.`DamageModifier`),"
        "b.`BaseAttackTime`,b.`RangeAttackTime`,b.`BaseVariance`,b.`RangeVariance`,"
        "COALESCE(d.`unit_class_override`, b.`unit_class`),"
        "b.`unit_flags`,b.`unit_flags2`,b.`dynamicflags`,b.`family`,b.`type`,b.`type_flags`,"
        "0,0,0,0,0,0,0,"
        "'',"
        "0,"
        "b.`HoverHeight`,"
        "COALESCE(d.`health_modifier_override`, b.`HealthModifier`),"
        "COALESCE(d.`mana_modifier_override`, b.`ManaModifier`),"
        "COALESCE(d.`armor_modifier_override`, b.`ArmorModifier`),"
        "b.`ExperienceModifier`,"
        "0,0,b.`RegenHealth`,b.`CreatureImmunitiesId`,b.`flags_extra`,'',b.`VerifiedBuild` "
        "FROM `lw_creature_template` d "
        "JOIN `creature_template` b ON b.`entry` = d.`base_creature_entry` "
        "WHERE d.`id` = " << lwTemplateId << " AND d.`enabled` = 1";

    ExecuteSql(sql.str());

    if (!WorldDatabase.Query(
        ("SELECT 1 FROM `creature_template` WHERE `entry` = " + std::to_string(allocatedEntry) + " LIMIT 1").c_str()))
    {
        return false;
    }

    std::ostringstream models;
    models <<
        "INSERT INTO `creature_template_model` "
        "(`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) "
        "SELECT " << allocatedEntry << ",`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild` "
        "FROM `creature_template_model` "
        "WHERE `CreatureID` = (SELECT `base_creature_entry` FROM `lw_creature_template` "
        "WHERE `id` = " << lwTemplateId << ")";

    ExecuteSql(models.str());

    if (!WorldDatabase.Query(
        ("SELECT 1 FROM `creature_template_model` WHERE `CreatureID` = " +
         std::to_string(allocatedEntry) + " LIMIT 1").c_str()))
    {
        LOG_ERROR("server.loading",
            "[LW Template] Logical template {} generated creature entry {} but no creature_template_model rows were copied.",
            lwTemplateId,
            allocatedEntry);
        return false;
    }

    std::ostringstream equipment;
    equipment <<
        "INSERT INTO `creature_equip_template` "
        "(`CreatureID`,`ID`,`ItemID1`,`ItemID2`,`ItemID3`,`VerifiedBuild`) "
        "SELECT " << allocatedEntry << ",`ID`,`ItemID1`,`ItemID2`,`ItemID3`,`VerifiedBuild` "
        "FROM `creature_equip_template` "
        "WHERE `CreatureID` = (SELECT `base_creature_entry` FROM `lw_creature_template` "
        "WHERE `id` = " << lwTemplateId << ")";

    ExecuteSql(equipment.str());

    LOG_INFO("server.loading",
        "[LW Template] Materialized logical template {} as creature entry {}.",
        lwTemplateId,
        allocatedEntry);

    return true;
}

void LwCreatureTemplateManager::LoadMappings()
{
    _entryByLwTemplate.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT m.`lw_template_id`, m.`allocated_entry` "
        "FROM `lw_creature_template_map` m "
        "JOIN `lw_creature_template` d ON d.`id` = m.`lw_template_id` "
        "JOIN `creature_template` c ON c.`entry` = m.`allocated_entry` "
        "WHERE m.`retired` = 0 AND d.`enabled` = 1");

    if (!result)
    {
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        _entryByLwTemplate.emplace(
            fields[0].Get<uint32>(),
            fields[1].Get<uint32>());
    } while (result->NextRow());
}

uint32 LwCreatureTemplateManager::ResolveEntry(uint32 lwTemplateId) const
{
    auto const itr = _entryByLwTemplate.find(lwTemplateId);
    return itr != _entryByLwTemplate.end() ? itr->second : 0;
}

uint32 LwCreatureTemplateManager::GetMappedTemplateCount() const
{
    return static_cast<uint32>(_entryByLwTemplate.size());
}
}
