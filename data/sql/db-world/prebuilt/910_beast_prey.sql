-- ============================================================================
-- Living World Hunt - legendary beast prey collection
--
-- All visuals and spells use existing 3.3.5a assets; no client patch is needed.
-- Combat abilities are data-driven through lw_hunt_prey_ability.
-- ============================================================================
SET @HUNT_ACTIVATOR_ENTRY := 14999010;

DELETE FROM `lw_hunt_prey_ability` WHERE `prey_id` BETWEEN 1 AND 8;
DELETE FROM `lw_hunt_prey` WHERE `id` BETWEEN 1 AND 8;
DELETE FROM `lw_creature_template` WHERE `id` BETWEEN 1000 AND 1007;

INSERT INTO `lw_creature_template`
 (`id`,`name`,`base_creature_entry`,`name_override`,`subname_override`,`faction_override`,`rank_override`,
  `health_modifier_override`,`armor_modifier_override`,`damage_modifier_override`,`enabled`,`comment`) VALUES
(1000,'Hunt Prey - Ashfang',448,'Ashfang','The Grey Terror',14,1,1.0,1.15,1.05,1,'Legendary wolf prey; Hogger supplies the existing client model shell.'),
(1001,'Hunt Prey - Silkmaw',30,'Silkmaw','The Webbed Horror',14,1,1.0,1.05,0.95,1,'Legendary spider prey; Forest Spider supplies the existing client model shell.'),
(1002,'Hunt Prey - Gorehide',157,'Gorehide','The Iron Tusk',14,1,1.0,1.25,1.10,1,'Legendary boar prey; Goretusk supplies the existing client model shell.'),
(1003,'Hunt Prey - Whiteclaw',1130,'Whiteclaw','The Mountain Ghost',14,1,1.0,1.25,1.10,1,'Legendary bear prey; Bjarn supplies the existing client model shell.'),
(1004,'Hunt Prey - Tidefang',391,'Tidefang','Terror of the Shore',14,1,1.0,1.10,1.15,1,'Legendary murloc prey; Old Murk-Eye supplies the existing client model shell.'),
(1005,'Hunt Prey - Stonegut',1210,'Stonegut','The Wandering Brute',14,1,1.0,1.30,1.20,1,'Legendary ogre prey; Chok''sul supplies the existing client model shell.'),
(1006,'Hunt Prey - Sootfang',1225,'Sootfang','The Black Bear',14,1,1.0,1.25,1.15,1,'Legendary bear prey; Ol'' Sooty supplies the existing client model shell.'),
(1007,'Hunt Prey - Nightfang',521,'Nightfang','The Shadow Wolf',14,1,1.0,1.15,1.20,1,'Legendary wolf prey; Lupos supplies the existing client model shell.');

INSERT INTO `lw_hunt_prey`
 (`id`,`name`,`min_level`,`max_level`,`prey_creature_entry`,`prey_lw_template_id`,`activation_gameobject_entry`,
  `ambush_health_multiplier`,`final_health_multiplier`,`escape_health_pct`,`ambush_count`,`enabled`,`comment`) VALUES
(1,'Ashfang',10,80,0,1000,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Baseline legendary beast prey.'),
(2,'Silkmaw',10,80,0,1001,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Control/poison legendary beast prey.'),
(3,'Gorehide',10,80,0,1002,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Charge/toughness legendary beast prey.'),
(4,'Whiteclaw',10,80,0,1003,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Heavy bear legendary beast prey.'),
(5,'Tidefang',10,80,0,1004,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Aggressive shoreline legendary beast prey.'),
(6,'Stonegut',10,80,0,1005,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'High-armor brute legendary prey.'),
(7,'Sootfang',10,80,0,1006,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Hard-hitting bear legendary beast prey.'),
(8,'Nightfang',10,80,0,1007,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Fast, dangerous wolf legendary beast prey.');

-- target: 0=victim, 1=self. encounter_mask: 1=ambush, 2=final, 3=both.
-- These known-good spell choices preserve the existing Silkmaw/Gorehide behavior.
INSERT INTO `lw_hunt_prey_ability`
 (`id`,`prey_id`,`spell_id`,`target`,`initial_min_ms`,`initial_max_ms`,`cooldown_min_ms`,`cooldown_max_ms`,`chance_pct`,`encounter_mask`,`enabled`,`comment`) VALUES
(2001,2,4167,0,5000,8000,18000,24000,100,3,1,'Silkmaw - Web'),
(2002,2,18197,0,2500,5000,12000,17000,80,3,1,'Silkmaw - Poison'),
(3001,3,25999,0,500,1500,18000,24000,100,3,1,'Gorehide - Charge');