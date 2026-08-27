-- Canonical clean-install schema for reusable runtime signal definitions.
-- Runtime emission state is held in memory by RuntimeSignalManager.
CREATE TABLE IF NOT EXISTS `lw_runtime_signal` (
    `id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(120) NOT NULL,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_lw_runtime_signal_name` (`name`),
    KEY `idx_lw_runtime_signal_enabled` (`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
