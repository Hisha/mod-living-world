-- Living World - generic world-event calendar
-- Permanent schema. No ALTER statements.

DROP TABLE IF EXISTS `lw_calendar_execution`;
DROP TABLE IF EXISTS `lw_calendar_action`;
DROP TABLE IF EXISTS `lw_calendar_rotation`;
DROP TABLE IF EXISTS `lw_calendar_schedule`;

CREATE TABLE `lw_calendar_schedule` (
  `id` int unsigned NOT NULL,
  `name` varchar(100) NOT NULL,
  `recurrence_type` tinyint unsigned NOT NULL COMMENT '1 one-time, 2 annual, 3 monthly weekday on/before day, 4 monthly weekday on/after day',
  `year` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'required for one-time; 0 otherwise',
  `month` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '1-12 for one-time/annual; ignored for monthly',
  `day` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'calendar day or monthly anchor day',
  `weekday` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0 Sunday .. 6 Saturday for monthly weekday rules',
  `hour` tinyint unsigned NOT NULL DEFAULT '0',
  `minute` tinyint unsigned NOT NULL DEFAULT '0',
  `catchup_seconds` int unsigned NOT NULL DEFAULT '86400' COMMENT 'restart/offline grace period for missed actions',
  `rotation_offset` int NOT NULL DEFAULT '0' COMMENT 'phase shift applied to occurrence ordinal before selecting rotation row',
  `enabled` tinyint unsigned NOT NULL DEFAULT '1',
  `comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `lw_calendar_rotation` (
  `id` int unsigned NOT NULL,
  `schedule_id` int unsigned NOT NULL,
  `rotation_order` smallint unsigned NOT NULL,
  `target_id_override` int unsigned NOT NULL DEFAULT '0' COMMENT 'optional action target override for this occurrence',
  `value` int unsigned NOT NULL DEFAULT '0' COMMENT 'generic site/variant value for future consumers',
  `label` varchar(100) DEFAULT NULL,
  `enabled` tinyint unsigned NOT NULL DEFAULT '1',
  `comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_lw_calendar_rotation_order` (`schedule_id`,`rotation_order`),
  CONSTRAINT `fk_lw_calendar_rotation_schedule` FOREIGN KEY (`schedule_id`) REFERENCES `lw_calendar_schedule` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `lw_calendar_action` (
  `id` int unsigned NOT NULL,
  `schedule_id` int unsigned NOT NULL,
  `action_order` smallint unsigned NOT NULL DEFAULT '0',
  `offset_days` int NOT NULL DEFAULT '0',
  `offset_minutes` int NOT NULL DEFAULT '0',
  `target_type` tinyint unsigned NOT NULL COMMENT '1 start invasion, 2 start traveling event, 3 stop traveling event',
  `target_id` int unsigned NOT NULL DEFAULT '0',
  `use_rotation_target` tinyint unsigned NOT NULL DEFAULT '0',
  `parameter1` int unsigned NOT NULL DEFAULT '0',
  `parameter2` int unsigned NOT NULL DEFAULT '0',
  `enabled` tinyint unsigned NOT NULL DEFAULT '1',
  `comment` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_lw_calendar_action_order` (`schedule_id`,`action_order`),
  CONSTRAINT `fk_lw_calendar_action_schedule` FOREIGN KEY (`schedule_id`) REFERENCES `lw_calendar_schedule` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `lw_calendar_execution` (
  `action_id` int unsigned NOT NULL,
  `occurrence_key` bigint NOT NULL,
  `scheduled_at` datetime NOT NULL,
  `executed_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `rotation_label` varchar(100) DEFAULT NULL,
  PRIMARY KEY (`action_id`,`occurrence_key`),
  CONSTRAINT `fk_lw_calendar_execution_action` FOREIGN KEY (`action_id`) REFERENCES `lw_calendar_action` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
