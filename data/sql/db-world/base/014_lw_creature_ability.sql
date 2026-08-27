-- ============================================================================
-- Living World - Creature Abilities
-- ============================================================================
-- Abilities are attached to logical lw_creature_template IDs, not allocated
-- creature_template entries. This keeps invasion packages portable.
--
-- target_type:
--   0 = self
--   1 = friendly LW creature with lowest health in the same runtime
--   2 = random qualifying friendly LW creature in the same runtime
--   3 = current hostile victim
--
-- health_threshold_pct is evaluated for self/friendly targets. 100 allows any
-- living target; 60 means the target must be at or below 60% health.
-- range_yards = 0 disables the explicit range check.
-- require_combat = 1 prevents out-of-combat casting.
-- ============================================================================

CREATE TABLE IF NOT EXISTS `lw_creature_ability` (
    `id` INT UNSIGNED NOT NULL,
    `lw_template_id` INT UNSIGNED NOT NULL,
    `spell_id` INT UNSIGNED NOT NULL,
    `target_type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `priority` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `health_threshold_pct` FLOAT UNSIGNED NOT NULL DEFAULT 100.0,
    `cooldown_ms` INT UNSIGNED NOT NULL DEFAULT 5000,
    `range_yards` FLOAT UNSIGNED NOT NULL DEFAULT 30.0,
    `require_combat` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
    `enabled` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL DEFAULT NULL,

    PRIMARY KEY (`id`),
    KEY `idx_lw_creature_ability_template` (`lw_template_id`),
    KEY `idx_lw_creature_ability_enabled` (`enabled`)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci;
  