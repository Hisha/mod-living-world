-- Canonical clean-install schema for runtime dialogue definitions.
-- chat_type: 0 = Say, 1 = Yell
-- language: AzerothCore Language enum value; 0 = LANG_UNIVERSAL.
CREATE TABLE IF NOT EXISTS `lw_dialogue` (
    `id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(120) NOT NULL,
    `text` VARCHAR(500) NOT NULL,
    `chat_type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `language` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL DEFAULT NULL,
    PRIMARY KEY (`id`),
    KEY `idx_lw_dialogue_enabled` (`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
