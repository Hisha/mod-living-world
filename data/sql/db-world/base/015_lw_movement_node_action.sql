-- Living World
-- Movement-node arrival actions.
--
-- Permanent/final schema only. No ALTER statements are required for the
-- GameObject actions; they use the existing generic action columns.
--
-- action_type:
--   1 = Dialogue
--       target_id  = lw_dialogue.id
--       parameter1 = speaker lw_spawn_member.id (0 = first available creature)
--       parameter2 = unused
--       parameter3 = unused
--   2 = Announcement
--       target_id  = lw_announcement.id
--       parameter1 = scope (0 global, 1 map, 2 zone, 3 area)
--       parameter2 = scope id (0 uses invasion default for map/zone)
--       parameter3 = faction (0 everyone, 1 alliance, 2 horde)
--   3 = Sound
--       target_id  = sound id
--       parameter1 = source lw_spawn_member.id (0 = first available creature)
--       parameter2 = playback mode (0 distance, 1 direct)
--       parameter3 = unused
--   4 = Open GameObject by DB spawn GUID
--       target_id  = gameobject DB spawn GUID
--       parameter1 = optional movement hold after opening, milliseconds
--       parameter2 = unused
--       parameter3 = unused
--   5 = Close/reset GameObject by DB spawn GUID
--       target_id  = gameobject DB spawn GUID
--       parameter1 = optional movement hold after closing, milliseconds
--       parameter2 = unused
--       parameter3 = unused
--
-- Note: action types 4/5 operate the server-side GameObject directly. They do
-- not perform a player interaction and do not remove or alter player locks,
-- keys, or requirements.

DROP TABLE IF EXISTS `lw_movement_node_action`;
CREATE TABLE `lw_movement_node_action` (
  `id` int unsigned NOT NULL,
  `node_id` int unsigned NOT NULL,
  `action_order` smallint unsigned NOT NULL DEFAULT '0',
  `action_type` tinyint unsigned NOT NULL,
  `target_id` int unsigned NOT NULL DEFAULT '0',
  `parameter1` int unsigned NOT NULL DEFAULT '0',
  `parameter2` int unsigned NOT NULL DEFAULT '0',
  `parameter3` int unsigned NOT NULL DEFAULT '0',
  `enabled` tinyint unsigned NOT NULL DEFAULT '1',
  `comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_lw_movement_node_action_order` (`node_id`,`action_order`),
  KEY `idx_lw_movement_node_action_node` (`node_id`),
  CONSTRAINT `fk_lw_movement_node_action_node`
    FOREIGN KEY (`node_id`) REFERENCES `lw_movement_node` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;