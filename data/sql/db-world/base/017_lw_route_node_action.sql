-- Invasion-specific actions triggered when a particular runtime spawn group
-- reaches (or spawns at) a semantic route node.
--
-- action_type 1 = Dialogue
--   target_id  = dialogue_id
--   parameter1 = speaker spawn_member_id (0 = first available creature)
--   parameter2 = reserved
--   parameter3 = reserved
--
-- action_type 2 = World Announcement
--   target_id  = announcement_id
--   parameter1 = scope: 0 global, 1 map, 2 zone, 3 area
--   parameter2 = scope_id (0 derives map/zone from invasion; area requires explicit id)
--   parameter3 = faction: 0 everyone, 1 Alliance, 2 Horde
--
-- action_type 3 = Sound
--   target_id  = sound_id
--   parameter1 = source spawn_member_id (0 = first available creature)
--   parameter2 = playback mode: 0 distance/positional, 1 direct
--   parameter3 = reserved
CREATE TABLE IF NOT EXISTS `lw_route_node_action` (
    `id` INT UNSIGNED NOT NULL,
    `invasion_id` INT UNSIGNED NOT NULL,
    `spawn_group_id` INT UNSIGNED NOT NULL,
    `route_node_id` INT UNSIGNED NOT NULL,
    `action_order` SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    `action_type` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `target_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `parameter1` INT UNSIGNED NOT NULL DEFAULT 0,
    `parameter2` INT UNSIGNED NOT NULL DEFAULT 0,
    `parameter3` INT UNSIGNED NOT NULL DEFAULT 0,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_lw_route_node_action_order` (`invasion_id`,`spawn_group_id`,`route_node_id`,`action_order`),
    KEY `idx_lw_route_node_action_lookup` (`invasion_id`,`spawn_group_id`),
    KEY `idx_lw_route_node_action_route_node` (`route_node_id`),
    CONSTRAINT `fk_lw_route_node_action_invasion`
      FOREIGN KEY (`invasion_id`) REFERENCES `lw_invasion` (`id`)
      ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT `fk_lw_route_node_action_spawn_group`
      FOREIGN KEY (`spawn_group_id`) REFERENCES `lw_spawn_group` (`id`)
      ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT `fk_lw_route_node_action_route_node`
      FOREIGN KEY (`route_node_id`) REFERENCES `lw_route_node` (`id`)
      ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
