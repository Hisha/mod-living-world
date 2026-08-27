-- ============================================================================
-- Living World Huntmasters - permanent capital-city service NPCs
-- No quest flags are used. Each Huntmaster is gossip-only and is discoverable
-- through the city's normal guard-direction gossip via lw_hunt_guard_locator.
-- ============================================================================
SET @SW_ENTRY := 14999980;
SET @IF_ENTRY := 14999981;
SET @DN_ENTRY := 14999982;
SET @EX_ENTRY := 14999983;
SET @OG_ENTRY := 14999984;
SET @TB_ENTRY := 14999985;
SET @UC_ENTRY := 14999986;
SET @SM_ENTRY := 14999987;

-- Existing race/city-appropriate visual shells.
SET @SW_BASE := 68;     -- Stormwind City Guard
SET @IF_BASE := 5595;   -- Ironforge Guard
SET @DN_BASE := 4262;   -- Darnassus Sentinel
SET @EX_BASE := 16733;  -- Exodar Peacekeeper
SET @OG_BASE := 3296;   -- Orgrimmar Grunt
SET @TB_BASE := 3084;   -- Bluffwatcher
SET @UC_BASE := 5624;   -- Undercity Guardian
SET @SM_BASE := 16222;  -- Silvermoon City Guardian

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (@SW_ENTRY,@IF_ENTRY,@DN_ENTRY,@EX_ENTRY,@OG_ENTRY,@TB_ENTRY,@UC_ENTRY,@SM_ENTRY);
DELETE FROM `creature_template` WHERE `entry` IN (@SW_ENTRY,@IF_ENTRY,@DN_ENTRY,@EX_ENTRY,@OG_ENTRY,@TB_ENTRY,@UC_ENTRY,@SM_ENTRY);

INSERT INTO `creature_template` (
    `entry`,`difficulty_entry_1`,`difficulty_entry_2`,`difficulty_entry_3`,`KillCredit1`,`KillCredit2`,`name`,`subname`,`IconName`,`gossip_menu_id`,
    `minlevel`,`maxlevel`,`exp`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`speed_swim`,`speed_flight`,`detection_range`,`rank`,`dmgschool`,
    `DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`dynamicflags`,`family`,`type`,`type_flags`,
    `lootid`,`pickpocketloot`,`skinloot`,`PetSpellDataId`,`VehicleId`,`mingold`,`maxgold`,`AIName`,`MovementType`,`HoverHeight`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,
    `ExperienceModifier`,`RacialLeader`,`movementId`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`VerifiedBuild`)
SELECT m.entry,0,0,0,0,0,m.npc_name,'Master of the Hunt',b.`IconName`,0,
    60,60,b.`exp`,b.`faction`,1,b.`speed_walk`,b.`speed_run`,b.`speed_swim`,b.`speed_flight`,b.`detection_range`,0,b.`dmgschool`,
    b.`DamageModifier`,b.`BaseAttackTime`,b.`RangeAttackTime`,b.`BaseVariance`,b.`RangeVariance`,b.`unit_class`,b.`unit_flags`,b.`unit_flags2`,
    b.`dynamicflags`,b.`family`,b.`type`,b.`type_flags`,0,0,0,0,0,0,0,'',0,b.`HoverHeight`,b.`HealthModifier`,b.`ManaModifier`,b.`ArmorModifier`,
    0,0,0,1,b.`CreatureImmunitiesId`,b.`flags_extra`,'lw_huntmaster',0
FROM (
    SELECT @SW_ENTRY entry,@SW_BASE base_entry,'Huntmaster Corvin' npc_name UNION ALL
    SELECT @IF_ENTRY,@IF_BASE,'Huntmaster Brannoc' UNION ALL
    SELECT @DN_ENTRY,@DN_BASE,'Huntmistress Shalara' UNION ALL
    SELECT @EX_ENTRY,@EX_BASE,'Huntmaster Veylan' UNION ALL
    SELECT @OG_ENTRY,@OG_BASE,'Huntmaster Gorrak' UNION ALL
    SELECT @TB_ENTRY,@TB_BASE,'Huntmaster Tahu' UNION ALL
    SELECT @UC_ENTRY,@UC_BASE,'Huntmaster Morcant' UNION ALL
    SELECT @SM_ENTRY,@SM_BASE,'Huntmistress Vaelith'
) m
JOIN `creature_template` b ON b.`entry`=m.base_entry;

INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT m.entry,ctm.`Idx`,ctm.`CreatureDisplayID`,ctm.`DisplayScale`,ctm.`Probability`,0
FROM (
    SELECT @SW_ENTRY entry,@SW_BASE base_entry UNION ALL SELECT @IF_ENTRY,@IF_BASE UNION ALL
    SELECT @DN_ENTRY,@DN_BASE UNION ALL SELECT @EX_ENTRY,@EX_BASE UNION ALL
    SELECT @OG_ENTRY,@OG_BASE UNION ALL SELECT @TB_ENTRY,@TB_BASE UNION ALL
    SELECT @UC_ENTRY,@UC_BASE UNION ALL SELECT @SM_ENTRY,@SM_BASE
) m
JOIN `creature_template_model` ctm ON ctm.`CreatureID`=m.base_entry;

-- Remove/recreate our permanent spawns. GUIDs are allocated above the current
-- database maximum at apply time so the module does not claim a fixed core GUID range.
DELETE FROM `creature` WHERE `id` IN (@SW_ENTRY,@IF_ENTRY,@DN_ENTRY,@EX_ENTRY,@OG_ENTRY,@TB_ENTRY,@UC_ENTRY,@SM_ENTRY);
SET @LW_HUNTMASTER_CGUID := (SELECT COALESCE(MAX(`guid`),0) FROM `creature`);
INSERT INTO `creature`
(`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curhealth`,`curmana`,`MovementType`,`npcflag`,`unit_flags`,`dynamicflags`,`ScriptName`,`VerifiedBuild`,`CreateObject`,`Comment`) VALUES
(@LW_HUNTMASTER_CGUID+1,@SW_ENTRY,0,0,0,1,1,0,-8831.55,628.72,94.02,3.14,300,0,0,1,0,0,0,0,0,'',0,0,'LW Huntmaster - Stormwind'),
(@LW_HUNTMASTER_CGUID+2,@IF_ENTRY,0,0,0,1,1,0,-4955.20,-1016.10,501.82,1.55,300,0,0,1,0,0,0,0,0,'',0,0,'LW Huntmaster - Ironforge'),
(@LW_HUNTMASTER_CGUID+3,@DN_ENTRY,1,0,0,1,1,0,9945.10,2497.20,1317.10,3.90,300,0,0,1,0,0,0,0,0,'',0,0,'LW Huntmaster - Darnassus'),
(@LW_HUNTMASTER_CGUID+4,@EX_ENTRY,530,0,0,1,1,0,-4115.37,-11688.30,-142.789,0.20,300,0,0,1,0,0,0,0,0,'',0,0,'LW Huntmaster - Exodar'),
(@LW_HUNTMASTER_CGUID+5,@OG_ENTRY,1,0,0,1,1,0,1514.70,-4416.30,22.20,4.70,300,0,0,1,0,0,0,0,0,'',0,0,'LW Huntmaster - Orgrimmar'),
(@LW_HUNTMASTER_CGUID+6,@TB_ENTRY,1,0,0,1,1,0,-1277.80,111.10,131.90,3.10,300,0,0,1,0,0,0,0,0,'',0,0,'LW Huntmaster - Thunder Bluff'),
(@LW_HUNTMASTER_CGUID+7,@UC_ENTRY,0,0,0,1,1,0,1585.90,239.40,-52.15,3.10,300,0,0,1,0,0,0,0,0,'',0,0,'LW Huntmaster - Undercity'),
(@LW_HUNTMASTER_CGUID+8,@SM_ENTRY,530,0,0,1,1,0,9484.20,-7279.10,14.30,3.20,300,0,0,1,0,0,0,0,0,'',0,0,'LW Huntmaster - Silvermoon');

DELETE FROM `lw_hunt_giver` WHERE `id` BETWEEN 1 AND 8;
INSERT INTO `lw_hunt_giver` (`id`,`creature_entry`,`city_name`,`map_id`,`x`,`y`,`z`,`enabled`,`comment`) VALUES
(1,@SW_ENTRY,'Stormwind City',0,-8831.55,628.72,94.02,1,'Alliance capital Huntmaster'),
(2,@IF_ENTRY,'Ironforge',0,-4955.20,-1016.10,501.82,1,'Alliance capital Huntmaster'),
(3,@DN_ENTRY,'Darnassus',1,9945.10,2497.20,1317.10,1,'Alliance capital Huntmaster'),
(4,@EX_ENTRY,'The Exodar',530,-4115.37,-11688.30,-142.789,1,'Alliance capital Huntmaster - Traders Tier'),
(5,@OG_ENTRY,'Orgrimmar',1,1514.70,-4416.30,22.20,1,'Horde capital Huntmaster'),
(6,@TB_ENTRY,'Thunder Bluff',1,-1277.80,111.10,131.90,1,'Horde capital Huntmaster'),
(7,@UC_ENTRY,'Undercity',0,1585.90,239.40,-52.15,1,'Horde capital Huntmaster'),
(8,@SM_ENTRY,'Silvermoon City',530,9484.20,-7279.10,14.30,1,'Horde capital Huntmaster');

DELETE FROM `lw_hunt_guard_locator` WHERE `id` BETWEEN 1 AND 11;
INSERT INTO `lw_hunt_guard_locator` (`id`,`guard_creature_entry`,`hunt_giver_id`,`enabled`,`comment`) VALUES
(1,68,1,1,'Stormwind City Guard'),
(2,1976,1,1,'Stormwind City Patroller'),
(3,5595,2,1,'Ironforge Guard'),
(4,4262,3,1,'Darnassus Sentinel'),
(5,16733,4,1,'Exodar Peacekeeper'),
(6,20674,4,1,'Shield of Velen'),
(7,3296,5,1,'Orgrimmar Grunt'),
(8,3084,6,1,'Bluffwatcher'),
(9,5624,7,1,'Undercity Guardian'),
(10,36213,7,1,'Kor''kron Overseer - Wrath-era Undercity'),
(11,16222,8,1,'Silvermoon City Guardian');
