-- Generic reusable camp layouts. Member placement is by logical member_key.
CREATE TABLE IF NOT EXISTS `lw_traveling_camp_layout` (
 `id` INT UNSIGNED NOT NULL, `name` VARCHAR(120) NOT NULL, `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
 `comment` VARCHAR(255) NULL, PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `lw_traveling_camp_layout_member` (
 `id` INT UNSIGNED NOT NULL, `layout_id` INT UNSIGNED NOT NULL, `member_key` VARCHAR(64) NOT NULL,
 `forward_offset` FLOAT NOT NULL DEFAULT 0, `right_offset` FLOAT NOT NULL DEFAULT 0, `z_offset` FLOAT NOT NULL DEFAULT 0,
 `orientation_offset` FLOAT NOT NULL DEFAULT 0, `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1, `comment` VARCHAR(255) NULL,
 PRIMARY KEY (`id`), UNIQUE KEY `uq_lw_travel_camp_member_key` (`layout_id`,`member_key`), KEY `idx_lw_travel_camp_member_layout` (`layout_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `lw_traveling_camp_layout_prop` (
 `id` INT UNSIGNED NOT NULL, `layout_id` INT UNSIGNED NOT NULL, `gameobject_entry` INT UNSIGNED NOT NULL,
 `forward_offset` FLOAT NOT NULL DEFAULT 0, `right_offset` FLOAT NOT NULL DEFAULT 0, `z_offset` FLOAT NOT NULL DEFAULT 0,
 `orientation_offset` FLOAT NOT NULL DEFAULT 0, `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1, `comment` VARCHAR(255) NULL,
 PRIMARY KEY (`id`), KEY `idx_lw_travel_camp_prop_layout` (`layout_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;