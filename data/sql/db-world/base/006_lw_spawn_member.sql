-- Canonical clean-install schema for entities contained in LW spawn groups.
-- entity_type values implemented by the engine:
--   1 = Creature
--   2 = GameObject
-- tactical_role values:
--   0 = Default
--   1 = Commander
--   2 = Protector (Tank / Protector)
--   3 = Melee DPS
--   4 = Ranged DPS
--   5 = Healer
--   6 = Support
CREATE TABLE IF NOT EXISTS `lw_spawn_member` (
    `id` INT UNSIGNED NOT NULL,
    `spawn_group_id` INT UNSIGNED NOT NULL,
    `entity_type` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `entity_entry` INT UNSIGNED NOT NULL,
    `lw_template_id` INT UNSIGNED NULL DEFAULT NULL,
    `count` SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    `level_override` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `tactical_role` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `comment` VARCHAR(255) NULL DEFAULT NULL,
    PRIMARY KEY (`id`),
    KEY `idx_lw_spawn_member_group` (`spawn_group_id`),
    KEY `idx_lw_spawn_member_type` (`entity_type`),
    KEY `idx_lw_spawn_member_lw_template` (`lw_template_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
