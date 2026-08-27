-- Living World
-- Shared reusable travel-route network.
--
-- Route nodes are logical connection points in the Living World travel graph.
-- They may represent road junctions, settlements, gates, bridges, docks, trail
-- intersections, or other meaningful travel connection points.
--
-- Route segments connect two route nodes using an existing lw_movement_path.
-- Segments are shared world infrastructure and do not belong to any specific
-- invasion or Living World event. Runtime consumers choose whether to traverse
-- a segment forward (start -> end) or reverse (end -> start).

CREATE TABLE IF NOT EXISTS `lw_route_node` (
  `id` int unsigned NOT NULL,
  `name` varchar(120) NOT NULL,
  `map_id` smallint unsigned NOT NULL,
  `x` float NOT NULL,
  `y` float NOT NULL,
  `z` float NOT NULL,
  `orientation` float NOT NULL DEFAULT '0',
  `arrival_radius` float unsigned NOT NULL DEFAULT '5',
  `enabled` tinyint unsigned NOT NULL DEFAULT '1',
  `comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_lw_route_node_name` (`name`),
  KEY `idx_lw_route_node_map` (`map_id`),
  KEY `idx_lw_route_node_enabled` (`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `lw_route_segment` (
  `id` int unsigned NOT NULL,
  `name` varchar(120) NOT NULL,
  `start_node_id` int unsigned NOT NULL,
  `end_node_id` int unsigned NOT NULL,
  `movement_path_id` int unsigned NOT NULL,
  `enabled` tinyint unsigned NOT NULL DEFAULT '1',
  `comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_lw_route_segment_name` (`name`),
  KEY `idx_lw_route_segment_start` (`start_node_id`),
  KEY `idx_lw_route_segment_end` (`end_node_id`),
  KEY `idx_lw_route_segment_path` (`movement_path_id`),
  KEY `idx_lw_route_segment_enabled` (`enabled`),
  CONSTRAINT `fk_lw_route_segment_start_node`
    FOREIGN KEY (`start_node_id`) REFERENCES `lw_route_node` (`id`),
  CONSTRAINT `fk_lw_route_segment_end_node`
    FOREIGN KEY (`end_node_id`) REFERENCES `lw_route_node` (`id`),
  CONSTRAINT `fk_lw_route_segment_movement_path`
    FOREIGN KEY (`movement_path_id`) REFERENCES `lw_movement_path` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
