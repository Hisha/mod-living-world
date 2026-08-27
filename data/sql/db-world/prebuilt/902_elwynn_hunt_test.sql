-- ============================================================================
-- Living World Hunt prototype content - Elwynn Forest
-- Huntmaster template is created but deliberately NOT permanently spawned.
-- For the first test place him with: .npc add 14999980
-- ============================================================================
SET @HUNTMASTER_ENTRY := 14999980;
SET @HUNTMASTER_BASE := 221; -- Dannus; known humanoid visual base
SET @PREY_BASE_ENTRY := 448;  -- Hogger visual/combat shell only
SET @PREY_LW_TEMPLATE_ID := 1000;
SET @HUNT_ACTIVATOR_ENTRY := 14999010;
SET @HUNT_ACTIVATOR_VISUAL_BASE := 190558; -- Shining Crystal visual

DELETE FROM `creature_template_model` WHERE `CreatureID`=@HUNTMASTER_ENTRY;
DELETE FROM `creature_template` WHERE `entry`=@HUNTMASTER_ENTRY;

INSERT INTO `creature_template` (
    `entry`,`difficulty_entry_1`,`difficulty_entry_2`,`difficulty_entry_3`,`KillCredit1`,`KillCredit2`,`name`,`subname`,`IconName`,`gossip_menu_id`,
    `minlevel`,`maxlevel`,`exp`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`speed_swim`,`speed_flight`,`detection_range`,`rank`,`dmgschool`,
    `DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`dynamicflags`,`family`,`type`,`type_flags`,
    `lootid`,`pickpocketloot`,`skinloot`,`PetSpellDataId`,`VehicleId`,`mingold`,`maxgold`,`AIName`,`MovementType`,`HoverHeight`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,
    `ExperienceModifier`,`RacialLeader`,`movementId`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`VerifiedBuild`)
SELECT @HUNTMASTER_ENTRY,0,0,0,0,0,'Huntmaster Corvin','Master of the Hunt',b.`IconName`,0,
    60,60,b.`exp`,35,1,b.`speed_walk`,b.`speed_run`,b.`speed_swim`,b.`speed_flight`,b.`detection_range`,0,b.`dmgschool`,
    b.`DamageModifier`,b.`BaseAttackTime`,b.`RangeAttackTime`,b.`BaseVariance`,b.`RangeVariance`,b.`unit_class`,b.`unit_flags`,b.`unit_flags2`,
    b.`dynamicflags`,b.`family`,b.`type`,b.`type_flags`,0,0,0,0,0,0,0,'',0,b.`HoverHeight`,b.`HealthModifier`,b.`ManaModifier`,b.`ArmorModifier`,
    0,0,0,1,b.`CreatureImmunitiesId`,b.`flags_extra`,'lw_huntmaster',b.`VerifiedBuild`
FROM `creature_template` b WHERE b.`entry`=@HUNTMASTER_BASE;

INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT @HUNTMASTER_ENTRY,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`
FROM `creature_template_model` WHERE `CreatureID`=@HUNTMASTER_BASE;

-- Hunt final activation object: custom goober using an existing client visual.
DELETE FROM `gameobject_template` WHERE `entry`=@HUNT_ACTIVATOR_ENTRY;
INSERT INTO `gameobject_template`
    (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,
     `Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,
     `Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,
     `Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,
     `AIName`,`ScriptName`,`VerifiedBuild`)
SELECT
    @HUNT_ACTIVATOR_ENTRY,10,`displayId`,'Prey Trail Marker','','Inspecting','','1.0',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    '','lw_hunt_activation',0
FROM `gameobject_template` WHERE `entry`=@HUNT_ACTIVATOR_VISUAL_BASE;

-- Ashfang is a real LW derived creature.  The client model still comes from
-- Hogger, but the visible name/rank/combat modifiers belong to Ashfang.
DELETE FROM `lw_creature_template` WHERE `id`=@PREY_LW_TEMPLATE_ID;
INSERT INTO `lw_creature_template`
    (`id`,`name`,`base_creature_entry`,`name_override`,`subname_override`,`faction_override`,`rank_override`,
     `health_modifier_override`,`armor_modifier_override`,`damage_modifier_override`,`enabled`,`comment`)
VALUES
    (@PREY_LW_TEMPLATE_ID,'Elwynn Hunt - Ashfang',@PREY_BASE_ENTRY,'Ashfang','Elwynn Quarry',14,1,
     4.0,1.25,1.35,1,'Prototype Hunt prey; derived from Hogger visual/base but fully LW-owned.');

DELETE FROM `lw_hunt_final_location` WHERE `hunt_id`=1;
DELETE FROM `lw_hunt_zone` WHERE `hunt_id`=1;
DELETE FROM `lw_hunt` WHERE `id`=1;
DELETE FROM `lw_hunt_giver` WHERE `id`=1 OR `creature_entry`=@HUNTMASTER_ENTRY;

INSERT INTO `lw_hunt` (`id`,`name`,`min_level`,`max_level`,`prey_creature_entry`,`prey_lw_template_id`,`activation_gameobject_entry`,`ambush_health_multiplier`,`final_health_multiplier`,`escape_health_pct`,`ambush_count`,`enabled`,`comment`) VALUES
(1,'Ashfang',10,80,0,@PREY_LW_TEMPLATE_ID,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Prototype prey hunt using an LW-derived Ashfang template and clickable final hunt marker.');

-- Elwynn Forest zone 12 is intentionally valid for every level 10+ during prototype testing.
INSERT INTO `lw_hunt_zone` (`id`,`hunt_id`,`zone_id`,`min_level`,`max_level`,`weight`,`enabled`,`comment`) VALUES
(1,1,12,10,80,100,1,'Prototype restriction: Elwynn Forest only.');

-- Authored final encounter sites. Coordinates can be refined in-game after the loop is proven.
INSERT INTO `lw_hunt_final_location` (`id`,`hunt_id`,`zone_id`,`map_id`,`x`,`y`,`z`,`orientation`,`location_name`,`weight`,`enabled`,`comment`) VALUES
(1,1,12,0,-9488.219727,67.639702,56.076900,0,'the outskirts of Goldshire',100,1,'Prototype site using verified LW route-node terrain'),
(2,1,12,0,-9657.429688,684.276001,37.414299,0,'Westbrook Garrison',100,1,'Prototype site using verified LW route-node terrain'),
(3,1,12,0,-9467.209961,-1273.780029,42.046200,0,'Eastvale Logging Camp',100,1,'Prototype site using verified LW route-node terrain');

INSERT INTO `lw_hunt_giver` (`id`,`creature_entry`,`city_name`,`team`,`enabled`,`comment`) VALUES
(1,@HUNTMASTER_ENTRY,'Stormwind City',1,1,'Prototype Alliance Huntmaster. Spawn manually for first test with .npc add 14999980.');
