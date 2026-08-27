-- Canonical clean-install schema: response origins must be created before invasions.
CREATE TABLE IF NOT EXISTS `lw_response_origin` (
    `id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(100) NOT NULL,
    `map_id` SMALLINT UNSIGNED NOT NULL,
    `team` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 = neutral, 1 = Alliance, 2 = Horde',
    `max_active_default` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '0 = unlimited',
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_lw_response_origin_name_map` (`name`, `map_id`),
    KEY `idx_lw_response_origin_map` (`map_id`),
    KEY `idx_lw_response_origin_enabled` (`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ===========================================================================
-- Response Origins Clear Data
-- ===========================================================================

DELETE FROM `lw_response_origin`
WHERE `id` IN (1,2,3,4,5,6,7,8);

-- ===========================================================================
-- Response Origins Defaults
-- ===========================================================================

INSERT INTO `lw_response_origin`
(
    `id`,
    `name`,
    `map_id`,
    `team`,
    `max_active_default`,
    `enabled`,
    `comment`
)
VALUES
    (1, 'Stormwind', 0, 1, 1, 1, 'Stormwind Response Settings.'),
    (2, 'Ironforge', 0, 1, 1, 1, 'Ironforge Response Settings.'),
    (3, 'Darnassus', 0, 1, 1, 1, 'Darnassus Response Settings.'),
    (4, 'The Exodar', 0, 1, 1, 1, 'The Exodar Response Settings.'),
    (5, 'Orgrimmar', 0, 1, 1, 1, 'Orgrimmar Response Settings.'),
    (6, 'Undercity', 0, 1, 1, 1, 'Undercity Response Settings.'),
    (7, 'Thunder Bluff', 0, 1, 1, 1, 'Thunder Bluff Response Settings.'),
    (8, 'Silvermoon City', 0, 1, 1, 1, 'Silvermoon City Response Settings.');
    
    
    
    