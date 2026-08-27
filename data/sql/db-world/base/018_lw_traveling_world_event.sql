-- Living World: generic non-combat traveling-world-event engine.
-- traversal_mode: 0 LOOP, 1 PING_PONG, 2 ONE_WAY.
-- auto_start: 1 starts at worldserver startup; calendar-controlled events use 0.

CREATE TABLE IF NOT EXISTS `lw_traveling_event` (
 `id` INT UNSIGNED NOT NULL, `name` VARCHAR(120) NOT NULL,
 `traversal_mode` TINYINT UNSIGNED NOT NULL DEFAULT 0,
 `auto_start` TINYINT UNSIGNED NOT NULL DEFAULT 0,
 `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1, `comment` VARCHAR(255) NULL,
 PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `lw_traveling_event_member` (
 `id` INT UNSIGNED NOT NULL, `event_id` INT UNSIGNED NOT NULL,
 `member_order` SMALLINT UNSIGNED NOT NULL DEFAULT 0, `member_key` VARCHAR(64) NOT NULL,
 `creature_entry` INT UNSIGNED NOT NULL, `is_leader` TINYINT UNSIGNED NOT NULL DEFAULT 0,
 `vendor_while_camped` TINYINT UNSIGNED NOT NULL DEFAULT 0, `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
 `comment` VARCHAR(255) NULL, PRIMARY KEY (`id`),
 UNIQUE KEY `uq_lw_travel_member_key` (`event_id`,`member_key`),
 UNIQUE KEY `uq_lw_travel_member_order` (`event_id`,`member_order`), KEY `idx_lw_travel_member_event` (`event_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `lw_traveling_event_stop` (
 `id` INT UNSIGNED NOT NULL, `event_id` INT UNSIGNED NOT NULL, `stop_order` INT UNSIGNED NOT NULL,
 `route_node_id` INT UNSIGNED NOT NULL, `camp_layout_id` INT UNSIGNED NOT NULL DEFAULT 0,
 `dwell_seconds` INT UNSIGNED NOT NULL DEFAULT 120, `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
 `comment` VARCHAR(255) NULL, PRIMARY KEY (`id`), UNIQUE KEY `uq_lw_travel_stop_order` (`event_id`,`stop_order`),
 KEY `idx_lw_travel_stop_event` (`event_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `lw_traveling_event_leg` (
 `id` INT UNSIGNED NOT NULL, `event_id` INT UNSIGNED NOT NULL, `from_stop_id` INT UNSIGNED NOT NULL,
 `to_stop_id` INT UNSIGNED NOT NULL, `speaker_member_id` INT UNSIGNED NOT NULL DEFAULT 0,
 `departure_text` VARCHAR(255) NOT NULL DEFAULT '', `arrival_text` VARCHAR(255) NOT NULL DEFAULT '',
 `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1, `comment` VARCHAR(255) NULL, PRIMARY KEY (`id`),
 UNIQUE KEY `uq_lw_travel_leg_direction` (`event_id`,`from_stop_id`,`to_stop_id`), KEY `idx_lw_travel_leg_event` (`event_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;