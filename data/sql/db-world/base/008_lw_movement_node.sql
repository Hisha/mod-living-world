CREATE TABLE IF NOT EXISTS `lw_movement_node`
(
    `id` INT UNSIGNED NOT NULL,
    `path_id` INT UNSIGNED NOT NULL,
    `node_order` SMALLINT UNSIGNED NOT NULL,

    `map_id` SMALLINT UNSIGNED NOT NULL,

    `x` FLOAT NOT NULL,
    `y` FLOAT NOT NULL,
    `z` FLOAT NOT NULL,
    `orientation` FLOAT NOT NULL DEFAULT 0,

    `wait_ms` INT UNSIGNED NOT NULL DEFAULT 0,

    `profile_override_id` INT UNSIGNED NOT NULL DEFAULT 0,

    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL DEFAULT NULL,

    PRIMARY KEY (`id`),

    UNIQUE KEY `uq_lw_movement_node_order`
        (`path_id`, `node_order`),

    KEY `idx_lw_movement_node_path`
        (`path_id`),

    KEY `idx_lw_movement_node_enabled`
        (`enabled`)
)
ENGINE=InnoDB
DEFAULT CHARSET=utf8mb4
COLLATE=utf8mb4_unicode_ci;