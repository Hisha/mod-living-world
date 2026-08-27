-- Canonical clean-install schema for stage actions.
-- action_type 1 = Spawn Group
-- action_type 2 = Start Route Journey
-- action_type 3 = Dialogue
-- action_type 4 = World Announcement
-- action_type 5 = Sound
-- action_type 6 = Spell (scripted event cast)
-- action_type 7 = Start Assault
-- action_type 8 = Watch Group Defeat
-- action_type 9 = Start Garrison Restock
--
-- Start Route Journey parameter mapping:
--   target_id  = spawn_group_id whose latest runtime entity group should move
--   parameter1 = start_route_node_id
--   parameter2 = destination_route_node_id
--   parameter3 = completion_signal_id (0 = emit no signal)
--
-- Invasion definitions never reference movement_path_id directly. The route graph
-- resolves the underlying route segments/movement paths between the two stable nodes.
--
-- Dialogue parameter mapping:
--   target_id  = spawn_group_id whose latest runtime entity group contains the speaker
--   parameter1 = dialogue_id
--   parameter2 = speaker spawn_member_id (0 = first available creature)
--   parameter3 = target policy bitmask: 1 quest givers, 2 vendors, 4 flight masters
--
-- World Announcement parameter mapping:
--   target_id  = announcement_id
--   parameter1 = scope: 0 global, 1 map, 2 zone, 3 area
--   parameter2 = scope_id (0 derives map/zone from invasion; area requires explicit id)
--   parameter3 = faction: 0 everyone, 1 Alliance, 2 Horde
CREATE TABLE IF NOT EXISTS `lw_stage_action` (
    `id` INT UNSIGNED NOT NULL,
    `stage_id` INT UNSIGNED NOT NULL,
    `action_order` SMALLINT UNSIGNED NOT NULL,
    `action_type` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `target_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `parameter1` INT UNSIGNED NOT NULL DEFAULT 0,
    `parameter2` INT UNSIGNED NOT NULL DEFAULT 0,
    `parameter3` INT UNSIGNED NOT NULL DEFAULT 0,
    `parameter4` INT UNSIGNED NOT NULL DEFAULT 0,
    `delay_seconds` INT UNSIGNED NOT NULL DEFAULT 0,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment` VARCHAR(255) NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_lw_stage_action_order` (`stage_id`, `action_order`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
--
-- Sound parameter mapping:
--   target_id  = spawn_group_id
--   parameter1 = sound_id (SoundEntries.dbc)
--   parameter2 = source spawn_member_id (0 = first available creature)
--   parameter3 = playback mode: 0 distance/positional, 1 direct
--
-- Spell parameter mapping (v1):
--   target_id  = caster spawn_group_id
--   parameter1 = spell_id
--   parameter2 = caster spawn_member_id (0 = first available creature)
--   parameter3 = target mode: 0 self
--
-- Spell Actions are explicit invasion-scripted casts only. Native creature
-- combat spells, automatic self-buffs, rotations, CreatureAI and SmartAI remain unchanged.


-- Start Assault parameter mapping:
--   target_id  = spawn_group_id whose latest runtime entity group should assault
--   parameter1 = search radius in yards (0 = 40)
--   parameter2 = target reacquire interval in milliseconds (0 = 2000; minimum 500)
--   parameter3 = target policy bitmask: 1 quest givers, 2 vendors
--   parameter4 = assault-center route_node_id (REQUIRED)
--
-- Assault uses AzerothCore hostility/attackability rules to select a target,
-- then explicitly calls the invader AI AttackStart. This lets invasion NPCs
-- initiate combat against valid settlement NPCs that may not proximity-aggro
-- the invaders themselves. Native combat AI/SmartAI controls the fight after
-- engagement begins.

-- Watch Group Defeat parameter mapping:
--   target_id  = spawn_group_id to watch
--   parameter1 = runtime signal id to emit
--   parameter2 = mode: 0 = ANY watched group defeated, 1 = ALL watched groups defeated
--   parameter3 = reserved
--
-- Multiple action_type 8 rows in the same runtime may register multiple spawn
-- groups into the same watch. They must use the same signal id and mode.

-- Start Garrison Restock parameter mapping:
--   target_id  = spawn_group_id whose latest runtime entity group should be maintained
--   parameter1 = quiet period in seconds before replenishment may begin (0 = 30)
--   parameter2 = maximum replacement creatures spawned per refill batch (0 = 5)
--   parameter3 = seconds between refill batches while the group remains quiet (0 = 10)
--
-- Garrison restock begins only after the runtime group has finished active movement.
-- Any active combat, assigned victim, or hostile player/playerbot near the garrison
-- resets the quiet timer. Replacements are spawned near the surviving group's
-- final/home objective position and are added back to the same runtime entity group.
-- Restock never exceeds the authored lw_spawn_member counts. A completely defeated
-- garrison does not regenerate from zero; at least one living creature must remain
-- to anchor the garrison.