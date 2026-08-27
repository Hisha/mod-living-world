CREATE TABLE IF NOT EXISTS `lw_movement_path`
(
    `id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(120) NOT NULL,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL DEFAULT NULL,

    PRIMARY KEY (`id`),
    KEY `idx_lw_movement_path_enabled` (`enabled`)
)
ENGINE=InnoDB
DEFAULT CHARSET=utf8mb4
COLLATE=utf8mb4_unicode_ci;