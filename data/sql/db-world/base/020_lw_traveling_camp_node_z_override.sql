-- Per-camp terrain Z corrections. target_type: 1 member-placement row, 2 prop-placement row.
CREATE TABLE IF NOT EXISTS `lw_traveling_camp_node_z_override` (
 `id` INT UNSIGNED NOT NULL, `route_node_id` INT UNSIGNED NOT NULL, `target_type` TINYINT UNSIGNED NOT NULL,
 `target_id` INT UNSIGNED NOT NULL, `z_override` FLOAT NOT NULL DEFAULT 0, `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
 `comment` VARCHAR(255) NULL, PRIMARY KEY (`id`), UNIQUE KEY `uq_lw_travel_camp_node_target` (`route_node_id`,`target_type`,`target_id`),
 KEY `idx_lw_travel_camp_node` (`route_node_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;