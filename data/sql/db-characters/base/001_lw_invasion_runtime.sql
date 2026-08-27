-- Realm/runtime state belongs in acore_characters rather than acore_world.
CREATE TABLE IF NOT EXISTS `lw_invasion_runtime` (
    `invasion_id` INT UNSIGNED NOT NULL,
    `state` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 = available, 1 = active, 2 = cooldown',
    `last_started_at` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `last_completed_at` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `next_eligible_at` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `active_since` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `active_until` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `times_started` INT UNSIGNED NOT NULL DEFAULT 0,
    `times_completed` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`invasion_id`),
    KEY `idx_lw_runtime_state` (`state`),
    KEY `idx_lw_runtime_next_eligible` (`next_eligible_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
