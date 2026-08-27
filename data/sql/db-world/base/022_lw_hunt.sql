-- Living World Hunt/Prey subsystem
CREATE TABLE IF NOT EXISTS `lw_hunt` (
  `id` INT UNSIGNED NOT NULL,
  `name` VARCHAR(100) NOT NULL,
  `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 10,
  `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80,
  `prey_creature_entry` MEDIUMINT UNSIGNED NOT NULL,
  `prey_lw_template_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `activation_gameobject_entry` MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
  `ambush_health_multiplier` FLOAT NOT NULL DEFAULT 4.0,
  `final_health_multiplier` FLOAT NOT NULL DEFAULT 6.0,
  `escape_health_pct` TINYINT UNSIGNED NOT NULL DEFAULT 50,
  `ambush_count` TINYINT UNSIGNED NOT NULL DEFAULT 2,
  `enabled` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
  `comment` VARCHAR(255) NULL,
  PRIMARY KEY (`id`),
  KEY `idx_lw_hunt_level` (`min_level`,`max_level`,`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `lw_hunt_zone` (
  `id` INT UNSIGNED NOT NULL,
  `hunt_id` INT UNSIGNED NOT NULL,
  `zone_id` INT UNSIGNED NOT NULL,
  `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80,
  `weight` INT UNSIGNED NOT NULL DEFAULT 100,
  `enabled` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
  `comment` VARCHAR(255) NULL,
  PRIMARY KEY (`id`),
  KEY `idx_lw_hunt_zone_hunt` (`hunt_id`,`enabled`),
  KEY `idx_lw_hunt_zone_zone` (`zone_id`,`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `lw_hunt_final_location` (
  `id` INT UNSIGNED NOT NULL,
  `hunt_id` INT UNSIGNED NOT NULL,
  `zone_id` INT UNSIGNED NOT NULL,
  `map_id` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `x` FLOAT NOT NULL,
  `y` FLOAT NOT NULL,
  `z` FLOAT NOT NULL,
  `orientation` FLOAT NOT NULL DEFAULT 0,
  `location_name` VARCHAR(120) NOT NULL,
  `weight` INT UNSIGNED NOT NULL DEFAULT 100,
  `enabled` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
  `comment` VARCHAR(255) NULL,
  PRIMARY KEY (`id`),
  KEY `idx_lw_hunt_final` (`hunt_id`,`zone_id`,`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `lw_hunt_giver` (
  `id` INT UNSIGNED NOT NULL,
  `creature_entry` MEDIUMINT UNSIGNED NOT NULL,
  `city_name` VARCHAR(80) NOT NULL,
  `team` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=both, 1=Alliance, 2=Horde',
  `enabled` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
  `comment` VARCHAR(255) NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_lw_hunt_giver_entry` (`creature_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
