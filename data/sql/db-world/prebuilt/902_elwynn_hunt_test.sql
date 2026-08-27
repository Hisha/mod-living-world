-- ============================================================================
-- Living World Hunt prototype content - Elwynn Forest
-- Huntmaster template is created but deliberately NOT permanently spawned.
-- For the first test place him with: .npc add 14999980
-- ============================================================================
SET @HUNTMASTER_ENTRY := 14999980;
SET @HUNTMASTER_BASE := 221; -- Dannus; known humanoid visual base
SET @PREY_ENTRY := 448;       -- Hogger; temporary prototype prey visual/combat base

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

DELETE FROM `lw_hunt_final_location` WHERE `hunt_id`=1;
DELETE FROM `lw_hunt_zone` WHERE `hunt_id`=1;
DELETE FROM `lw_hunt` WHERE `id`=1;
DELETE FROM `lw_hunt_giver` WHERE `id`=1 OR `creature_entry`=@HUNTMASTER_ENTRY;

INSERT INTO `lw_hunt` (`id`,`name`,`min_level`,`max_level`,`prey_creature_entry`,`escape_health_pct`,`ambush_count`,`enabled`,`comment`) VALUES
(1,'Ashfang',10,80,@PREY_ENTRY,50,2,1,'Prototype prey hunt; Hogger is used only as the temporary prey base during engine testing.');

-- Elwynn Forest zone 12 is intentionally valid for every level 10+ during prototype testing.
INSERT INTO `lw_hunt_zone` (`id`,`hunt_id`,`zone_id`,`min_level`,`max_level`,`weight`,`enabled`,`comment`) VALUES
(1,1,12,10,80,100,1,'Prototype restriction: Elwynn Forest only.');

-- Authored final encounter sites. Coordinates can be refined in-game after the loop is proven.
INSERT INTO `lw_hunt_final_location` (`id`,`hunt_id`,`zone_id`,`map_id`,`x`,`y`,`z`,`orientation`,`location_name`,`weight`,`enabled`,`comment`) VALUES
(1,1,12,0,-9465.0,64.0,56.0,0,'the woods east of Goldshire',100,1,'Prototype site'),
(2,1,12,0,-9840.0,920.0,29.0,0,'the western Elwynn woods',100,1,'Prototype site'),
(3,1,12,0,-9135.0,-1050.0,71.0,0,'the road toward Eastvale',100,1,'Prototype site');

INSERT INTO `lw_hunt_giver` (`id`,`creature_entry`,`city_name`,`team`,`enabled`,`comment`) VALUES
(1,@HUNTMASTER_ENTRY,'Stormwind City',1,1,'Prototype Alliance Huntmaster. Spawn manually for first test with .npc add 14999980.');
