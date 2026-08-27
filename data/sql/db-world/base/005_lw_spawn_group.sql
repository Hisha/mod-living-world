-- Invasion-facing spawn locations are stable route-node anchors.
-- Raw XYZ remains an internal runtime implementation detail only.
CREATE TABLE IF NOT EXISTS `lw_spawn_group` (
    `id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(120) NOT NULL,
    `route_node_id` INT UNSIGNED NOT NULL,
    `spawn_radius` FLOAT NOT NULL DEFAULT 5,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`id`),
    KEY `idx_lw_spawn_group_route_node` (`route_node_id`),
    KEY `idx_lw_spawn_group_enabled` (`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
