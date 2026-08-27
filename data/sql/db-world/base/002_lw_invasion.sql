-- Canonical clean-install schema. Logical relationship:
-- lw_invasion.response_origin_id -> lw_response_origin.id
CREATE TABLE IF NOT EXISTS `lw_invasion` (
    `id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(120) NOT NULL,
    `map_id` SMALLINT UNSIGNED NOT NULL,
    `zone_id` INT UNSIGNED NOT NULL,
    `team` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 = neutral, 1 = Alliance, 2 = Horde',
    `response_origin_id` INT UNSIGNED NOT NULL,
    `recommended_min_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `recommended_max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80,
    `selection_weight` INT UNSIGNED NOT NULL DEFAULT 100,
    `minimum_cooldown_seconds` INT UNSIGNED NOT NULL DEFAULT 86400,
    `maximum_cooldown_seconds` INT UNSIGNED NOT NULL DEFAULT 604800,
    `maximum_runtime_seconds` INT UNSIGNED NOT NULL DEFAULT 1800 COMMENT 'Hard safety limit for one runtime; must be greater than 0',
    `allow_random_start` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `comment` VARCHAR(255) NULL DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_lw_invasion_name` (`name`),
    KEY `idx_lw_invasion_enabled` (`enabled`),
    KEY `idx_lw_invasion_map` (`map_id`),
    KEY `idx_lw_invasion_zone` (`zone_id`),
    KEY `idx_lw_invasion_response_origin` (`response_origin_id`),
    KEY `idx_lw_invasion_random_selection` (`enabled`, `allow_random_start`, `map_id`),
    CONSTRAINT `chk_lw_invasion_level_range` CHECK (`recommended_min_level` <= `recommended_max_level`),
    CONSTRAINT `chk_lw_invasion_cooldown_range` CHECK (`minimum_cooldown_seconds` <= `maximum_cooldown_seconds`),
    CONSTRAINT `chk_lw_invasion_max_runtime` CHECK (`maximum_runtime_seconds` > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
