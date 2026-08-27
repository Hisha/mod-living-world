-- Canonical clean-install schema: stages are loaded after invasion definitions.
-- completion_type:
--   0 = timer
--   1 = runtime signal
--   2 = objective (reserved)
--   3 = manual (reserved)
-- completion_target_id:
--   timer: 0
--   runtime signal: lw_runtime_signal.id
CREATE TABLE IF NOT EXISTS `lw_invasion_stage` (
    `id` INT UNSIGNED NOT NULL,
    `invasion_id` INT UNSIGNED NOT NULL,
    `stage_order` SMALLINT UNSIGNED NOT NULL,
    `name` VARCHAR(120) NOT NULL,
    `duration_seconds` INT UNSIGNED NOT NULL DEFAULT 30,
    `completion_type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `completion_target_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_lw_invasion_stage_order` (`invasion_id`, `stage_order`),
    KEY `idx_lw_invasion_stage_invasion` (`invasion_id`),
    KEY `idx_lw_invasion_stage_enabled` (`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;