-- ===========================================================================
-- lw_invasion Clear Data
-- ===========================================================================
DELETE FROM lw_invasion
WHERE id IN (1);

-- ===========================================================================
-- lw_invasion_stage Clear Data
-- ===========================================================================
DELETE FROM lw_invasion_stage
WHERE id IN (1001,1002,1003,1004,1005,1006);

-- ===========================================================================
-- lw_stage_action Clear Data
-- ===========================================================================
DELETE FROM lw_stage_action
WHERE id IN (10001,10002,10003,10004,10005,10006,10007,10008,10009,10010,10011,10012,10013,10014,10015,10016,10017,10018,10019,10020,10021,10022,10023,10024,10025,10026,10027,10028,10029,10030,10031,10032,10033,10034,10035,10036,10037,10038);

-- ===========================================================================
-- lw_spawn_group Clear Data
-- ===========================================================================
DELETE FROM lw_spawn_group
WHERE id IN (100,101,102,103,104,105,106,107);

-- ===========================================================================
-- lw_creature_template Clear Data
-- ===========================================================================
DELETE FROM lw_creature_ability
WHERE id IN (100,101);

-- ===========================================================================
-- lw_creature_template Clear Data
-- ===========================================================================
DELETE FROM lw_creature_template
WHERE id IN (1,2,3,4,5,6);

-- ===========================================================================
-- lw_spawn_member Clear Data
-- ===========================================================================
DELETE FROM lw_spawn_member
WHERE id IN (100001,100002,100003,100004,100005,100006,100007,100008,100009,100010,100011,100012,100013,100014,100015,100016,100017,100018,100019);

-- ===========================================================================
-- lw_movement_path Clear Data
-- ===========================================================================
DELETE FROM lw_movement_path
WHERE id IN (100,101,102,103,104,105);

-- ===========================================================================
-- lw_movement_node Clear Data
-- ===========================================================================
DELETE FROM lw_movement_node
WHERE id IN (10000,10001,10002,10003,10004,10005,10006,10007,10008,10009,10010,10011,10012,10013,10014,10015,10016,10017,10501,10502,10503,10504,10505,10506,10507,10508,10509,10510,10511,10512,10513,10514,10515,10516,10517,10518,10519,10520,10521,10522,10523,10524,10525,10526,10527,10528,10529,10530,10531,10532);

-- ===========================================================================
-- lw_movement_profile Clear Data
-- ===========================================================================
DELETE FROM lw_movement_profile
WHERE id IN (100);

-- ===========================================================================
-- lw_runtime_signal Clear Data
-- ===========================================================================
DELETE FROM lw_runtime_signal
WHERE id IN (100,101,102,103,104);

-- ===========================================================================
-- lw_dialogue Clear Data
-- ===========================================================================
DELETE FROM lw_dialogue
WHERE id IN (100,101,102,103);

-- ===========================================================================
-- lw_announcement Clear Data
-- ===========================================================================
DELETE FROM lw_announcement
WHERE id IN (100,101,102,103);

-- ===========================================================================
-- Legacy lw_movement_node_action Clear Data
-- ===========================================================================
DELETE FROM lw_movement_node_action
WHERE id IN (20001,20002,20003,20004);

-- ===========================================================================
-- lw_route_node_action Clear Data
-- ===========================================================================
DELETE FROM lw_route_node_action
WHERE id IN (20001,20002,20003,20004);

-- ===========================================================================
-- lw_invasion table(Name the invasion):
-- ===========================================================================
INSERT INTO lw_invasion(id,name,map_id,zone_id,team,response_origin_id, recommended_min_level,recommended_max_level,selection_weight,minimum_cooldown_seconds,maximum_cooldown_seconds,maximum_runtime_seconds,allow_random_start,enabled,comment) 
VALUES (1, 'Defias Westfall Invasion', 0, 40, 1, 1, 10, 20, 100, 79200, 115200, 3600, 1, 1, 'Defias attack/control Sentinel Hill');

-- ===========================================================================
-- lw_invasion_stage(The stages of the invasion):
-- ===========================================================================
INSERT INTO lw_invasion_stage(id,invasion_id,stage_order,name,duration_seconds,completion_type,completion_target_id,enabled,comment) 
VALUES (1001, 1, 10, 'Defias Scouts', 0, 1, 100, 1, 'Defias Scouts establish staging point over looking Sentinel Hill'),
       (1002, 1, 20, 'Defias Populate Staging', 10, 0, 0, 1, 'Defias populate strike force at staging point.'),
       (1003, 1, 30, 'Defias Establish Control', 600, 0, 0, 1, 'Defias establish control of Sentinel Hill'),
       (1004, 1, 40, 'Defias Leadership Arrives', 10, 0, 0, 1, 'Defias Leadership arrives at Sentinel Hill'),
       (1005, 1, 50, 'Stormwind Response', 0, 1, 103, 1, 'Stormwind response forces heads to Sentinel Hill'),
       (1006, 1, 60, 'Stormwind vs Defias', 0, 1, 104, 1, 'Final battle to destroy the Defias at Sentinel Hill');

-- ===========================================================================
-- lw_stage_action():
-- ===========================================================================
INSERT INTO lw_stage_action(id,stage_id,action_order,action_type,target_id,parameter1,parameter2,parameter3,parameter4,delay_seconds,enabled,comment)
VALUES (10001, 1001, 1, 1, 100, 0, 0, 0, 0, 0, 1, 'Spawn Defias Scout group.'),
	   (10002, 1001, 2, 1, 101, 0, 0, 0, 0, 0, 1, 'Spawn Defias 2nd Scout group.'),
       (10003, 1001, 3, 2, 100, 150, 170, 100, 0, 0, 1, 'Route Defias Scout Group from Defias_Scouts to Defias_Scout_Staging and emit ScoutRouteComplete'),
       (10004, 1001, 4, 2, 101, 150, 180, 100, 0, 0, 1, 'Route Defias 2nd Scout Group from Defias_Scouts to Defias_2nd_Scout_Staging and emit ScoutRouteComplete'),
       (10005, 1002, 1, 3, 100, 100, 0, 0, 0, 0, 1, 'A scout says a warning.'),
       (10006, 1002, 2, 1, 102, 0, 0, 0, 0, 0, 1, 'Spawn Defias Control team.'),
       (10007, 1002, 3, 1, 103, 0, 0, 0, 0, 0, 1, 'Spawn Defias 2nd Control team.'),
       (10008, 1002, 4, 1, 104, 0, 0, 0, 0, 0, 1, 'Spawn Defias 3rd Control team.'),
       (10009, 1003, 1, 4, 100, 2, 0, 1, 0, 0, 1, 'Alliance-only Westfall zone warning; zone id derives from invasion.'),
       (10010, 1003, 2, 2, 100, 170, 70, 0, 0, 0, 1, 'Route Defias Scout group from Defias_Scout_Staging to Sentinel_Hill_Tower.'),
       (10011, 1003, 3, 2, 101, 180, 190, 0, 0, 0, 1, 'Route Defias 2nd Scout group from Defias_2nd_Scout_Staging to Defias_2nd_Group_AP.'),
       (10012, 1003, 4, 2, 102, 170, 70, 0, 0, 0, 1, 'Route Defias Control group from Defias_Scout_Staging to Sentinel_Hill_Tower.'),
       (10013, 1003, 5, 2, 103, 180, 190, 0, 0, 0, 1, 'Route Defias 2nd Control group from Defias_2nd_Scout_Staging to Defias_2nd_Group_AP.'),
       (10014, 1003, 6, 2, 104, 180, 200, 0, 0, 0, 1, 'Route Defias 3rd Control group from Defias_2nd_Scout_Staging to Defias_3rd_Group_AP.'),
       (10015, 1003, 7, 7, 100, 75, 2000, 7, 70, 0, 1, 'Defias Scouts assault Sentinel Hill.'),
       (10016, 1003, 8, 7, 101, 75, 2000, 7, 230, 0, 1, 'Defias 2nd Scouts assault Sentinel Hill.'),
       (10017, 1003, 9, 7, 102, 75, 2000, 7, 70, 0, 1, 'Defias Control Team assaults Sentinel Hill.'),
       (10018, 1003, 10, 7, 103, 75, 2000, 7, 230, 0, 1, 'Defias 2nd Control Team assaults Sentinel Hill.'),
       (10019, 1003, 11, 7, 104, 75, 2000, 7, 60, 0, 1, 'Defias 3rd Control Team assaults Sentinel Hill.'),
       (10020, 1003, 12, 9, 102, 30, 5, 10, 0, 0, 1, 'Defias Control Team garrisons Sentinel Hill and replenishes after 30 seconds uncontested.'),
       (10021, 1003, 13, 9, 103, 30, 5, 10, 0, 0, 1, 'Defias 2nd Control Team garrisons Sentinel Hill and replenishes after 30 seconds uncontested.'),
       (10022, 1003, 14, 9, 104, 30, 5, 10, 0, 0, 1, 'Defias 3rd Control Team garrisons Sentinel Hill and replenishes after 30 seconds uncontested.'),
       (10023, 1004, 1, 1, 105, 0, 0, 0, 0, 0, 1, 'Spawn Defias Leadership.'),
       (10024, 1004, 2, 3, 105, 101, 100012, 0, 0, 0, 1, 'Defias Leadership yells a warning.'),
       (10025, 1004, 3, 7, 105, 75, 2000, 7, 70, 0, 1, 'Defias Leadership assaults Sentinel Hill.'),
       (10026, 1005, 1, 1, 106, 0, 0, 0, 0, 0, 1, 'Spawn Stormwind Response Force.'),
       (10027, 1005, 2, 2, 106, 140, 70, 103, 0, 0, 1, 'Route main Stormwind Response Force from Stormwind_Response_Force_Spawn to Sentinel_Hill_Tower.'),
       (10036, 1005, 3, 1, 107, 0, 0, 0, 0, 0, 1, 'Spawn Stormwind Sentinel Hill Inn detachment.'),
       (10037, 1005, 4, 2, 107, 140, 230, 0, 0, 0, 1, 'Route Stormwind Inn detachment with the main column until Sentinel_Hill_Split, then branch to Sentinel_Hill_Inn.'),
       (10028, 1006, 1, 3, 106, 103, 100014, 0, 0, 0, 1, 'Commander Aldric yells the battle cry.'),
       (10029, 1006, 2, 7, 106, 100, 2000, 0, 70, 0, 1, 'Main Stormwind Response Force assaults the Defias.'),
       (10038, 1006, 3, 7, 107, 60, 2000, 0, 230, 0, 1, 'Stormwind Inn detachment secures the Sentinel Hill inn area.'),
       (10030, 1006, 4, 8, 100, 104, 1, 0, 0, 0, 1, 'Stormwind victory watch - Defias Scouts'),
       (10031, 1006, 5, 8, 101, 104, 1, 0, 0, 0, 1, 'Stormwind victory watch - Defias 2nd Scouts'),
       (10032, 1006, 6, 8, 102, 104, 1, 0, 0, 0, 1, 'Stormwind victory watch - Defias Control Team'),
       (10033, 1006, 7, 8, 103, 104, 1, 0, 0, 0, 1, 'Stormwind victory watch - Defias 2nd Control Team'),
       (10034, 1006, 8, 8, 104, 104, 1, 0, 0, 0, 1, 'Stormwind victory watch - Defias 3rd Control Team'),
       (10035, 1006, 9, 8, 105, 104, 1, 0, 0, 0, 1, 'Stormwind victory watch - Defias Leadership');

-- ===========================================================================
-- lw_spawn_group(Spawn anchors are stable lw_route_node IDs from 801_routes.sql.):
-- ===========================================================================
INSERT INTO lw_spawn_group(id,name,route_node_id,spawn_radius,enabled)
VALUES (100, 'Defias Scouts', 150, 10, 1),
       (101, 'Defias 2nd Scouts', 150, 10, 1),
       (102, 'Defias Control Team', 170, 10, 1),
       (103, 'Defias 2nd Control Team', 180, 10, 1),
       (104, 'Defias 3rd Control Team', 180, 10, 1),
       (105, 'Defias Leadership', 70, 10, 1),
       (106, 'Stormwind Response Force', 140, 10, 1),
       (107, 'Stormwind Sentinel Hill Inn Detachment', 140, 10, 1);

-- ===========================================================================
-- lw_creature_template(Create custom NPCs to be able to rename/reRank/etc an existing NPC.):
-- ===========================================================================
INSERT INTO lw_creature_template(id,name,base_creature_entry,name_override,subname_override,faction_override,rank_override,health_modifier_override,armor_modifier_override,damage_modifier_override,enabled,comment) 
VALUES (1, 'Defias Ogre Brute', 644, 'Defias Ogre Brute', 'Defias Brotherhood', 17, 0, 3.0, 1.5, NULL, 1, 'Defias Ogre based on Rhahk Zor'),
	   (2, 'Westfall Invasion Boss', 639, 'Captain Garrick Vane', 'Defias Field Commander', 17, 1, 5.0, 1.5, 2.0, 1, 'Westfall Invasion Leader based on Edwin VanCleef'),
       (3, 'Defias Field Medic', 4418, 'Defias Field Medic', 'Defias Brotherhood', 17, 0, 1.0, 1.0, 0.7, 1, 'Defias healer based on Defias Wizard'),
       (4, 'Commander Aldric Stoneward', 466, 'Commander Aldric Stoneward', 'Stormwind Response Force', 12, 0, 5.0, 1.5, 2.0, 1, 'Stormwind Response Force Commander based on General Marcus Jonathan'),
       (5, 'Stormwind Battle Chaplain', 5484, 'Stormwind Battle Chaplain', 'Stormwind Response Force', 12, 0, 1.0, 1.0, 0.7, 1, 'Stormwind Healer based on Brother Benjamin'),
       (6, 'Stormwind Paladin', 4885, 'Stormwind Paladin', 'Stormwind Response Force', 12, 0, 1.0, 1.0, 0.7, 1, 'Stormwind Paladin based on Duthorian Rall');
       
-- ===========================================================================
-- lw_creature_ability(Data-driven combat abilities for LW templates.):
-- ===========================================================================
INSERT INTO lw_creature_ability(id,lw_template_id,spell_id,target_type,priority,health_threshold_pct,cooldown_ms,range_yards,require_combat,enabled,comment)
VALUES (100, 3, 2054, 1, 100, 70.0, 4000, 30.0, 1, 1, 'Defias Field Medic heals the lowest-health LW ally at or below 70%.'),
       (101, 5, 2054, 1, 100, 70.0, 4000, 30.0, 1, 1, 'Stormwind Battle Chaplain heals the lowest-health LW ally at or below 70%.');

-- ===========================================================================
-- lw_spawn_member(Gives the spawn list of NPCs being used in invasion.):
-- ===========================================================================
INSERT INTO lw_spawn_member(id,spawn_group_id,entity_type,entity_entry,lw_template_id,count,level_override,tactical_role,comment) 
VALUES (100001, 100, 1, 449, NULL, 3, 30, 3, 'Defias scouts - Melee DPS'),
	   (100002, 101, 1, 449, NULL, 3, 30, 3, 'Defias 2nd scouts - Melee DPS'),
       (100003, 102, 1, 589, NULL, 15, 30, 4, 'Defias Control Team - Ranged DPS'),
       (100004, 102, 1, 0, 3, 10, 30, 5, 'Defias Control Team - Healer'),
       (100005, 102, 1, 0, 1, 5, 30, 3, 'Defias Control Team - Melee DPS'),
       (100006, 103, 1, 589, NULL, 15, 30, 4, 'Defias 2nd Control Team - Ranged DPS'),
       (100007, 103, 1, 0, 3, 10, 30, 5, 'Defias 2nd Control Team - Healer'),
       (100008, 103, 1, 0, 1, 5, 30, 3, 'Defias 2nd Control Team - Melee DPS'),
       (100009, 104, 1, 589, NULL, 15, 30, 4, 'Defias 3rd Control Team - Ranged DPS'),
       (100010, 104, 1, 0, 3, 10, 30, 5, 'Defias 3rd Control Team - Healer'),
       (100011, 104, 1, 0, 1, 5, 30, 3, 'Defias 3rd Control Team - Melee DPS'),
       (100012, 105, 1, 0, 2, 1, 35, 1, 'Defias Leadership - Melee DPS'),
       (100013, 106, 1, 68, NULL, 25, 25, 3, 'Stormwind Guard - Melee DPS (main column)'),
       (100014, 106, 1, 0, 4, 1, 35, 1, 'Stormwind Commander - Melee DPS'),
       (100015, 106, 1, 0, 5, 8, 30, 5, 'Stormwind Battle Chaplain - Healer (main column)'),
       (100016, 106, 1, 0, 6, 10, 30, 3, 'Stormwind Paladin - Melee DPS (main column)'),
       (100017, 107, 1, 68, NULL, 10, 25, 3, 'Stormwind Inn Detachment Guard - Melee DPS'),
       (100018, 107, 1, 0, 5, 5, 30, 5, 'Stormwind Inn Detachment Battle Chaplain - Healer'),
       (100019, 107, 1, 0, 6, 5, 30, 3, 'Stormwind Inn Detachment Paladin - Melee DPS');

-- ===========================================================================
-- Movement paths/nodes are no longer invasion-owned.
-- All invasion movement resolves through stable route-node IDs in 801_routes.sql.
-- ===========================================================================

-- ===========================================================================
-- Movement profiles are no longer invasion-facing; route journeys use the route network defaults.
-- ===========================================================================

-- ===========================================================================
-- lw_runtime_signal(Signals to indicate completeion of task.):
-- ===========================================================================
INSERT INTO lw_runtime_signal(id,name,enabled,comment) 
VALUES (100, 'ScoutRouteComplete', 1, 'Emitted when the Defias scout movement route reaches staging point.'),
       (101, 'StagingComplete', 1, 'Emitted when the Defias spawns attack force at staging point.'),
       (102, 'LeadershipComplete', 1, 'Emitted when the Defias spawns leadership at Sentinel Hill.'),
       (103, 'StormwindResponseComplete', 1, 'Emitted when the Stormwind Response arrives at Sentinel Hill.'),
       (104, 'StormwindWins', 1, 'Emitted when the Stormwind Response secures Sentinel Hill.');

-- ===========================================================================
-- lw_dialogue(Say/Yells by the NPCs):
-- ===========================================================================
INSERT INTO lw_dialogue(id,name,text,chat_type,language,enabled,comment)
VALUES (100, 'Defias Scout Warning', 'Keep your eyes open. Sentinel Hill is ahead.', 0, 0, 1, 'Defias Scout say at staging point.'),
       (101, 'Defias Leadership Warning', 'The Brotherhood have taken Westfall!', 1, 0, 1, 'Defias Leadership announcing they control Westfall.'),
       (102, 'Commander Aldric Stoneward urges the column onward', 'Keep up men, Westfall needs our help!', 1, 0, 1, 'Commander Aldric Stoneward urges the column onward'),
       (103, 'Commander Aldric Stoneward sounds the battle cry', 'Soldiers of Stormwind! Drive these Defias dogs from Sentinel Hill! For Westfall! For the Alliance!', 1, 0, 1, 'Commander Aldric Stoneward sounds the battle cry');

-- ===========================================================================
-- lw_announcement(Announcements to the world, limited by stage action parameters):
-- ===========================================================================
INSERT INTO lw_announcement(id,name,text,enabled,comment)
VALUES (100, 'Westfall Alliance Warning', 'Defias activity has been reported near Sentinel Hill. Alliance forces in Westfall are advised to remain alert.', 1, 'Westfall area Defias warning.'),
       (101, 'Stormwind Response Force Leaving Stormwind', 'To arms! A Stormwind response force marches for Westfall. All able-bodied members of the Alliance are called to Sentinel Hill to aid in the fight against the Defias!', 1, 'Stormwind Response Force Leaving Stormwind.'),
       (102, 'Stormwind Response Force Rally Troops', 'To arms! A Stormwind response force marches for Westfall. All able-bodied members of the Alliance are called to Sentinel Hill to aid in the fight against the Defias!', 1, 'Stormwind Response Force Rally Troops.'),
       (103, 'Stormwind Response Force Enters Westfall', 'Stormwind Response Force has arrived in Westfall. All able-bodied Alliance members rally at Sentinel Hill!', 1, 'Stormwind Response Force Enters Westfall.');

-- ===========================================================================
-- lw_route_node_action(Invasion-scoped actions triggered at semantic route anchors.):
-- ===========================================================================
INSERT INTO lw_route_node_action(id,invasion_id,spawn_group_id,route_node_id,action_order,action_type,target_id,parameter1,parameter2,parameter3,enabled,comment)
VALUES (20001, 1, 106, 140, 1, 2, 101, 1, 0, 1, 1, 'Stormwind Response Force departing Stormwind announcement at Stormwind_Response_Force_Spawn.'),
       (20002, 1, 106, 20,  1, 2, 102, 2, 12, 1, 1, 'Stormwind Response Force rally troops announcement at Goldshire.'),
       (20003, 1, 106, 30,  1, 1, 102, 100014, 0, 0, 1, 'Commander urges his troops forward at Westbrook_Split.'),
       (20004, 1, 106, 210, 1, 2, 103, 2, 0, 1, 1, 'Stormwind Response Force entered Westfall announcement at Westfall_Entrance_Announce.');
