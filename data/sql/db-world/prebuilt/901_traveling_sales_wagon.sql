-- ============================================================================
-- 901_Traveling_Sales_Wagon.sql
-- Traveling salesman: custom merchant + two creature-based pack mules.
-- Server-side only; no client patch required.
-- ============================================================================

SET @EVENT_ID := 1;
SET @MERCHANT_ENTRY := 14999990;
SET @MERCHANT_BASE := 221;       -- Dannus: display 23 / merchant visual
SET @PACK_MULE_ENTRY := 5525;    -- Caravan Packhorse
SET @CAMP_LAYOUT_ID := 1;         -- Reusable Traveling Salesman camp

-- LW-owned custom camp GameObjects.
SET @GO_SALESMAN_TENT := 14999001;      -- clones stock 180031
SET @GO_SALESMAN_CRATE := 14999002;     -- clones stock 271
SET @GO_SALESMAN_CAMPFIRE := 14999003;  -- clones stock 1798

-- --------------------------------------------------------------------------
-- Custom LW-owned camp GameObject templates.
-- One custom ID per logical asset; individual placements remain separate
-- layout-prop rows.
-- --------------------------------------------------------------------------
DELETE FROM `gameobject_template`
WHERE `entry` IN (@GO_SALESMAN_TENT,@GO_SALESMAN_CRATE,@GO_SALESMAN_CAMPFIRE);

INSERT INTO `gameobject_template`
    (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,
     `Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,
     `Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,
     `Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,
     `AIName`,`ScriptName`,`VerifiedBuild`)
SELECT
    @GO_SALESMAN_TENT,`type`,`displayId`,'LW Traveling Salesman Tent',
    `IconName`,`castBarCaption`,`unk1`,`size`,
    `Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,
    `Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,
    `Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,
    `AIName`,`ScriptName`,`VerifiedBuild`
FROM `gameobject_template` WHERE `entry` = 180031;

INSERT INTO `gameobject_template`
    (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,
     `Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,
     `Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,
     `Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,
     `AIName`,`ScriptName`,`VerifiedBuild`)
SELECT
    @GO_SALESMAN_CRATE,`type`,`displayId`,'LW Traveling Salesman Crates',
    `IconName`,`castBarCaption`,`unk1`,`size`,
    `Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,
    `Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,
    `Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,
    `AIName`,`ScriptName`,`VerifiedBuild`
FROM `gameobject_template` WHERE `entry` = 271;

INSERT INTO `gameobject_template`
    (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,
     `Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,
     `Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,
     `Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,
     `AIName`,`ScriptName`,`VerifiedBuild`)
SELECT
    @GO_SALESMAN_CAMPFIRE,`type`,`displayId`,'LW Traveling Salesman Campfire',
    `IconName`,`castBarCaption`,`unk1`,`size`,
    `Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,
    `Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,
    `Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,
    `AIName`,`ScriptName`,`VerifiedBuild`
FROM `gameobject_template` WHERE `entry` = 1798;

-- --------------------------------------------------------------------------
-- Custom Traveling Salesman creature.
-- Keep it below LW's generated-creature allocation range (15000000+).
-- Rebuild safely each time this prebuilt is applied.
-- --------------------------------------------------------------------------
DELETE FROM `npc_vendor` WHERE `entry` = @MERCHANT_ENTRY;
DELETE FROM `creature_template_model` WHERE `CreatureID` = @MERCHANT_ENTRY;
DELETE FROM `creature_template` WHERE `entry` = @MERCHANT_ENTRY;

INSERT INTO `creature_template` (
    `entry`,`difficulty_entry_1`,`difficulty_entry_2`,`difficulty_entry_3`,
    `KillCredit1`,`KillCredit2`,`name`,`subname`,`IconName`,`gossip_menu_id`,
    `minlevel`,`maxlevel`,`exp`,`faction`,`npcflag`,`speed_walk`,`speed_run`,
    `speed_swim`,`speed_flight`,`detection_range`,`rank`,`dmgschool`,
    `DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,
    `unit_class`,`unit_flags`,`unit_flags2`,`dynamicflags`,`family`,`type`,`type_flags`,
    `lootid`,`pickpocketloot`,`skinloot`,`PetSpellDataId`,`VehicleId`,`mingold`,`maxgold`,
    `AIName`,`MovementType`,`HoverHeight`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,
    `ExperienceModifier`,`RacialLeader`,`movementId`,`RegenHealth`,`CreatureImmunitiesId`,
    `flags_extra`,`ScriptName`,`VerifiedBuild`)
SELECT
    @MERCHANT_ENTRY,0,0,0,0,0,
    'Traveling Salesman','Traveling Merchant',b.`IconName`,0,
    b.`minlevel`,b.`maxlevel`,b.`exp`,b.`faction`,128,
    b.`speed_walk`,b.`speed_run`,b.`speed_swim`,b.`speed_flight`,b.`detection_range`,
    b.`rank`,b.`dmgschool`,b.`DamageModifier`,b.`BaseAttackTime`,b.`RangeAttackTime`,
    b.`BaseVariance`,b.`RangeVariance`,b.`unit_class`,b.`unit_flags`,b.`unit_flags2`,
    b.`dynamicflags`,b.`family`,b.`type`,b.`type_flags`,0,0,0,0,0,0,0,'',0,
    b.`HoverHeight`,b.`HealthModifier`,b.`ManaModifier`,b.`ArmorModifier`,
    b.`ExperienceModifier`,0,0,b.`RegenHealth`,b.`CreatureImmunitiesId`,
    b.`flags_extra`,'',b.`VerifiedBuild`
FROM `creature_template` b
WHERE b.`entry` = @MERCHANT_BASE;

INSERT INTO `creature_template_model`
    (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT @MERCHANT_ENTRY,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`
FROM `creature_template_model`
WHERE `CreatureID` = @MERCHANT_BASE;

-- Basic road/general-goods stock. The runtime removes the vendor flag while
-- traveling and restores it only while camped.
INSERT INTO `npc_vendor`
    (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`VerifiedBuild`)
VALUES
    (@MERCHANT_ENTRY,0,117,  0,0,0,NULL),   -- Tough Jerky
    (@MERCHANT_ENTRY,1,159,  0,0,0,NULL),   -- Refreshing Spring Water
    (@MERCHANT_ENTRY,2,4496, 0,0,0,NULL),   -- Small Brown Pouch
    (@MERCHANT_ENTRY,3,2320, 0,0,0,NULL),   -- Coarse Thread
    (@MERCHANT_ENTRY,4,2321, 0,0,0,NULL),   -- Fine Thread
    (@MERCHANT_ENTRY,5,3371, 0,0,0,NULL),   -- Empty Vial
    (@MERCHANT_ENTRY,6,17705, 1,86400,0,NULL),  -- Thrash Blade
    (@MERCHANT_ENTRY,7,2164, 1,86400,0,NULL);   -- Gut Ripper

-- --------------------------------------------------------------------------
-- Generic Traveling Salesman reference event.
-- traversal_mode 1 = PING_PONG; auto_start 1 = persistent worldserver startup.
-- --------------------------------------------------------------------------
DELETE FROM `lw_traveling_camp_node_z_override` WHERE `route_node_id` IN (240,250,260,650,660,670);
DELETE FROM `lw_traveling_event_leg` WHERE `event_id`=@EVENT_ID;
DELETE FROM `lw_traveling_event_stop` WHERE `event_id`=@EVENT_ID;
DELETE FROM `lw_traveling_event_member` WHERE `event_id`=@EVENT_ID;
DELETE FROM `lw_traveling_camp_layout_prop` WHERE `layout_id`=@CAMP_LAYOUT_ID;
DELETE FROM `lw_traveling_camp_layout_member` WHERE `layout_id`=@CAMP_LAYOUT_ID;
DELETE FROM `lw_traveling_camp_layout` WHERE `id`=@CAMP_LAYOUT_ID;
DELETE FROM `lw_traveling_event` WHERE `id`=@EVENT_ID;

INSERT INTO `lw_traveling_event` (`id`,`name`,`traversal_mode`,`auto_start`,`enabled`,`comment`) VALUES
(@EVENT_ID,'Traveling Salesman',1,1,1,'Generic reference traveler: salesman plus two pack mules.');

INSERT INTO `lw_traveling_event_member` (`id`,`event_id`,`member_order`,`member_key`,`creature_entry`,`is_leader`,`vendor_while_camped`,`enabled`,`comment`) VALUES
(901001,@EVENT_ID,10,'merchant',@MERCHANT_ENTRY,1,1,1,'Traveling Salesman / route leader'),
(901002,@EVENT_ID,20,'pack_mule_left',@PACK_MULE_ENTRY,0,0,1,'Pack Mule #1'),
(901003,@EVENT_ID,30,'pack_mule_right',@PACK_MULE_ENTRY,0,0,1,'Pack Mule #2');

INSERT INTO `lw_traveling_camp_layout` (`id`,`name`,`enabled`,`comment`) VALUES
(@CAMP_LAYOUT_ID,'Traveling Salesman Basic Camp',1,'Reusable member-key-based roadside camp.');

INSERT INTO `lw_traveling_camp_layout_member` (`id`,`layout_id`,`member_key`,`forward_offset`,`right_offset`,`z_offset`,`orientation_offset`,`enabled`,`comment`) VALUES
(901001,@CAMP_LAYOUT_ID,'merchant',0.5,1.5,0,3.14159,1,'Merchant'),
(901002,@CAMP_LAYOUT_ID,'pack_mule_left',-3.0,-7.5,0,0,1,'Pack Mule #1'),
(901003,@CAMP_LAYOUT_ID,'pack_mule_right',-4.5,-9.0,0,0,1,'Pack Mule #2');

INSERT INTO `lw_traveling_camp_layout_prop` (`id`,`layout_id`,`gameobject_entry`,`forward_offset`,`right_offset`,`z_offset`,`orientation_offset`,`enabled`,`comment`) VALUES
(901101,@CAMP_LAYOUT_ID,@GO_SALESMAN_TENT,4.0,0.0,0,0,1,'Food Tent - purple/white'),
(901102,@CAMP_LAYOUT_ID,@GO_SALESMAN_CRATE,4.0,-3.0,0,0,1,'Crates - left of tent'),
(901103,@CAMP_LAYOUT_ID,@GO_SALESMAN_CRATE,4.0,3.0,0,0,1,'Crates - right of tent'),
(901104,@CAMP_LAYOUT_ID,@GO_SALESMAN_CAMPFIRE,-6.0,0,0,0,1,'Camp Fire');

INSERT INTO `lw_traveling_camp_node_z_override` (`id`,`route_node_id`,`target_type`,`target_id`,`z_override`,`enabled`,`comment`) VALUES
(901201,240,2,901104,-0.75,1,'Goldshire: lower campfire'),
(901202,260,2,901104,-0.75,1,'Sentinel Hill lower campfire'),
(901203,260,1,901002,0.50,1,'Sentinel Hill raise Pack Mule #1'),
(901204,260,1,901003,0.50,1,'Sentinel Hill raise Pack Mule #2'),
(901205,260,2,901102,1.00,1,'Sentinel Hill raise left crates'),
(901206,260,2,901103,0.50,1,'Sentinel Hill raise right crates'),
(901207,250,2,901102,0.50,1,'Stormwind raise left crates'),
(901208,250,2,901104,-0.75,1,'Stormwind lower campfire'),
(901209,670,2,901104,-0.35,1,'Darkshire lower campfire'),
(901210,670,2,901102,0.50,1,'Darkshire raise left crates'),
(901211,670,2,901103,0.35,1,'Darkshire raise right crates'),
(901212,660,2,901104,-2.00,1,'Lakeshire lower campfire'),
(901213,660,2,901102,1.00,1,'Lakeshire raise left crates'),
(901214,660,2,901103,1.00,1,'Lakeshire raise right crates'),
(901215,660,1,901001,0.25,1,'Lakeshire raise merchant'),
(901216,650,2,901104,0.50,1,'South Gate Pass raise campfire'),
(901217,650,1,901002,0.25,1,'South Gate Pass raise Pack Mule #1'),
(901218,650,1,901003,0.25,1,'South Gate Pass raise Pack Mule #2');

INSERT INTO `lw_traveling_event_stop` (`id`,`event_id`,`stop_order`,`route_node_id`,`camp_layout_id`,`dwell_seconds`,`enabled`,`comment`) VALUES
(90101,@EVENT_ID,10,250,@CAMP_LAYOUT_ID,30,1,'Stormwind Gate camp'),
(90102,@EVENT_ID,20,240,@CAMP_LAYOUT_ID,30,1,'Goldshire camp'),
(90103,@EVENT_ID,30,260,@CAMP_LAYOUT_ID,30,1,'Sentinel Hill camp'),
(90104,@EVENT_ID,40,670,@CAMP_LAYOUT_ID,30,1,'Darkshire camp'),
(90105,@EVENT_ID,50,660,@CAMP_LAYOUT_ID,30,1,'Lakeshire camp'),
(90106,@EVENT_ID,60,650,@CAMP_LAYOUT_ID,30,1,'South Gate Pass camp');

INSERT INTO `lw_traveling_event_leg` (`id`,`event_id`,`from_stop_id`,`to_stop_id`,`speaker_member_id`,`departure_text`,`arrival_text`,`enabled`,`comment`) VALUES
(901301,@EVENT_ID,90101,90102,901001,'Come along, you two. Goldshire is next.','Fresh goods from Stormwind! Have a look while we rest.',1,'Stormwind -> Goldshire'),
(901302,@EVENT_ID,90102,90103,901001,'Time to get moving. Westfall is waiting.','Sentinel Hill! Supplies for anyone who needs them.',1,'Goldshire -> Sentinel Hill'),
(901303,@EVENT_ID,90103,90104,901001,'Let us walk quickly past Raven Hill and get to Darkshire.','Darkshire finally. We will rest here for a moment.',1,'Sentinel Hill -> Darkshire'),
(901304,@EVENT_ID,90104,90105,901001,'Lakeshire is our next stop.','Lake Everstill is calming. This is a nice place to rest.',1,'Darkshire -> Lakeshire'),
(901305,@EVENT_ID,90105,90106,901001,'Keep up, you two. South Gate Pass is a hike.','This cold is getting to my bones. We will stop here for now.',1,'Lakeshire -> South Gate Pass'),
(901306,@EVENT_ID,90106,90105,901001,'Back down the mountain. Lakeshire is next.','Back at Lake Everstill. We will rest a moment.',1,'South Gate Pass -> Lakeshire'),
(901307,@EVENT_ID,90105,90104,901001,'Darkshire is our next stop.','Darkshire again. Keep an eye on the road while we rest.',1,'Lakeshire -> Darkshire'),
(901308,@EVENT_ID,90104,90103,901001,'Westfall is next. Let us get moving.','Sentinel Hill again! Supplies while we take a short break.',1,'Darkshire -> Sentinel Hill'),
(901309,@EVENT_ID,90103,90102,901001,'Back toward Goldshire.','Goldshire again! Have a look while we rest.',1,'Sentinel Hill -> Goldshire'),
(901310,@EVENT_ID,90102,90101,901001,'Stormwind is our next stop.','Back at the Stormwind gate. We will camp here for a bit.',1,'Goldshire -> Stormwind');