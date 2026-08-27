-- Canonical clean-install schema for Living World announcements.
-- Announcements are reusable text definitions; delivery scope/faction are
-- configured by the stage action that invokes them.
CREATE TABLE IF NOT EXISTS `lw_announcement` (
    `id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(120) NOT NULL,
    `text` VARCHAR(500) NOT NULL,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL DEFAULT NULL,
    PRIMARY KEY (`id`),
    KEY `idx_lw_announcement_enabled` (`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;