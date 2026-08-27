-- Active stage progress belongs in acore_characters so a running invasion can survive a worldserver restart.
CREATE TABLE IF NOT EXISTS `lw_active_runtime` (
    `runtime_id` BIGINT UNSIGNED NOT NULL,
    `invasion_id` INT UNSIGNED NOT NULL,
    `current_stage_id` INT UNSIGNED NOT NULL,
    `stage_started_at` BIGINT UNSIGNED NOT NULL,
    `stage_ends_at` BIGINT UNSIGNED NOT NULL,
    `started_at` BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (`runtime_id`),
    UNIQUE KEY `uq_lw_active_runtime_invasion` (`invasion_id`),
    KEY `idx_lw_active_runtime_stage_end` (`stage_ends_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;