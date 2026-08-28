-- ============================================================================
-- Living World Hunt - first legendary beast prey collection
-- Ashfang is the baseline melee quarry; Silkmaw and Gorehide prove the
-- data-driven prey ability layer. All visuals/spells are existing 3.3.5 assets.
-- ============================================================================
SET @HUNT_ACTIVATOR_ENTRY := 14999010;

DELETE FROM `lw_hunt_prey_ability` WHERE `prey_id` IN (1,2,3);
DELETE FROM `lw_hunt_prey` WHERE `id` IN (1,2,3);
DELETE FROM `lw_creature_template` WHERE `id` IN (1000,1001,1002);

INSERT INTO `lw_creature_template`
 (`id`,`name`,`base_creature_entry`,`name_override`,`subname_override`,`faction_override`,`rank_override`,
  `health_modifier_override`,`armor_modifier_override`,`damage_modifier_override`,`enabled`,`comment`) VALUES
(1000,'Hunt Prey - Ashfang',448,'Ashfang','The Grey Terror',14,1,1.0,1.15,1.05,1,'Legendary wolf prey; Hogger supplies the existing client model shell.'),
(1001,'Hunt Prey - Silkmaw',30,'Silkmaw','The Webbed Horror',14,1,1.0,1.05,0.95,1,'Legendary spider prey; Forest Spider supplies the existing client model shell.'),
(1002,'Hunt Prey - Gorehide',157,'Gorehide','The Iron Tusk',14,1,1.0,1.25,1.10,1,'Legendary boar prey; Goretusk supplies the existing client model shell.');

INSERT INTO `lw_hunt_prey`
 (`id`,`name`,`min_level`,`max_level`,`prey_creature_entry`,`prey_lw_template_id`,`activation_gameobject_entry`,
  `ambush_health_multiplier`,`final_health_multiplier`,`escape_health_pct`,`ambush_count`,`enabled`,`comment`) VALUES
(1,'Ashfang',10,19,0,1000,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Baseline low-level legendary beast prey.'),
(2,'Silkmaw',10,19,0,1001,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Control/poison low-level legendary beast prey.'),
(3,'Gorehide',10,19,0,1002,@HUNT_ACTIVATOR_ENTRY,4.0,6.0,50,2,1,'Charge/toughness low-level legendary beast prey.');

-- target: 0=victim, 1=self. encounter_mask: 1=ambush, 2=final, 3=both.
-- Silkmaw: Web roots for 4 sec; Poison is a level-10-era periodic Nature effect.
-- Gorehide: Charge is the existing boar charge ability.
INSERT INTO `lw_hunt_prey_ability`
 (`id`,`prey_id`,`spell_id`,`target`,`initial_min_ms`,`initial_max_ms`,`cooldown_min_ms`,`cooldown_max_ms`,`chance_pct`,`encounter_mask`,`enabled`,`comment`) VALUES
(2001,2,4167,0,5000,8000,18000,24000,100,3,1,'Silkmaw - Web'),
(2002,2,18197,0,2500,5000,12000,17000,80,3,1,'Silkmaw - Poison'),
(3001,3,25999,0,500,1500,18000,24000,100,3,1,'Gorehide - Charge');
