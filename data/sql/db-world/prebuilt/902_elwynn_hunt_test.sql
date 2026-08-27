-- ============================================================================
-- Living World Hunt prototype content - Ashfang + Elwynn Forest
-- ============================================================================
SET @PREY_BASE_ENTRY := 448;  -- Hogger visual/combat shell only
SET @PREY_LW_TEMPLATE_ID := 1000;
SET @HUNT_ACTIVATOR_ENTRY := 14999010;
SET @HUNT_ACTIVATOR_VISUAL_BASE := 190558; -- Shining Crystal visual

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

DELETE FROM `lw_creature_template` WHERE `id`=@PREY_LW_TEMPLATE_ID;
INSERT INTO `lw_creature_template`
    (`id`,`name`,`base_creature_entry`,`name_override`,`subname_override`,`faction_override`,`rank_override`,
     `health_modifier_override`,`armor_modifier_override`,`damage_modifier_override`,`enabled`,`comment`)
VALUES
    (@PREY_LW_TEMPLATE_ID,'Elwynn Hunt - Ashfang',@PREY_BASE_ENTRY,'Ashfang','Elwynn Quarry',14,1,
     4.0,1.25,1.35,1,'Prototype Hunt prey; derived from Hogger visual/base but fully LW-owned.');

DELETE FROM `lw_hunt` WHERE `id`=1;
INSERT INTO `lw_hunt` (`id`,`name`,`min_level`,`max_level`,`prey_creature_entry`,`prey_lw_template_id`,`activation_gameobject_entry`,`ambush_health_multiplier`,`final_health_multiplier`,`escape_health_pct`,`ambush_count`,`enabled`,`comment`) VALUES
(1,'Ashfang',10,80,0,@PREY_LW_TEMPLATE_ID,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Prototype prey used while the reusable prey catalog is built.');

-- Elwynn remains wide-open for prototype testing. World-zone data in 904 is
-- level-appropriate; this row intentionally overrides Elwynn to level 80.
DELETE FROM `lw_hunt_zone` WHERE `zone_id`=12;
INSERT INTO `lw_hunt_zone` (`id`,`zone_id`,`map_id`,`name`,`min_level`,`max_level`,`weight`,`enabled`,`comment`) VALUES
(1,12,0,'Elwynn Forest',10,80,100,1,'Prototype override: Elwynn remains available to every level 10+ hunter.');

DELETE FROM `lw_hunt_final_location` WHERE `zone_id`=12;
INSERT INTO `lw_hunt_final_location` (`id`,`zone_id`,`map_id`,`x`,`y`,`z`,`orientation`,`location_name`,`weight`,`enabled`,`comment`) VALUES
(1,12,0,-9198.709961,-613.720947,60.858318,0,'the foothills outside Jasperlode Mine',100,1,'Wilderness final site; intentionally away from faction settlements and city guards'),
(2,12,0,-9331.179688,-986.290527,66.546524,0,'the shore of Stone Cairn Lake',100,1,'Wilderness final site; intentionally away from faction settlements and city guards'),
(3,12,0,-9764.944336,-836.307190,39.463924,0,'Brackwell Pumpkin Patch',100,1,'Wilderness final site; intentionally away from faction settlements and city guards');
