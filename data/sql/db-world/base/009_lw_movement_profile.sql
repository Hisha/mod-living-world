CREATE TABLE IF NOT EXISTS `lw_movement_profile`
(
    `id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(120) NOT NULL,

    `default_mode` TINYINT UNSIGNED NOT NULL DEFAULT 0,

    `walk_speed_multiplier` FLOAT NOT NULL DEFAULT 1.0,
    `run_speed_multiplier` FLOAT NOT NULL DEFAULT 1.0,

    `stealth_enabled` TINYINT UNSIGNED NOT NULL DEFAULT 0,

    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL DEFAULT NULL,

    PRIMARY KEY (`id`),

    KEY `idx_lw_movement_profile_enabled`
        (`enabled`)
)
ENGINE=InnoDB
DEFAULT CHARSET=utf8mb4
COLLATE=utf8mb4_unicode_ci;