#include "LivingWorld.h"

#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"
#include <algorithm>

namespace lw
{
LivingWorldDataMgr& LivingWorldDataMgr::Instance()
{
    static LivingWorldDataMgr instance;
    return instance;
}

void LivingWorldDataMgr::Clear()
{
    _responseOrigins.clear();
    _definitions.clear();
    _stagesByInvasion.clear();
    _actionsByStage.clear();
    _spawnGroups.clear();
    _spawnMembersByGroup.clear();
    _movementPaths.clear();
    _movementNodesByPath.clear();
    _movementNodeActionsByNode.clear();
    _movementProfiles.clear();
    _routeNodes.clear();
    _routeSegments.clear();
    _routeNodeActionsByInvasionGroup.clear();
    _runtimeSignals.clear();
    _dialogues.clear();
    _announcements.clear();
}

void LivingWorldDataMgr::LoadDefinitions()
{
    Clear();

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `name`, `map_id`, `team`, `max_active_default`, `enabled` "
        "FROM `lw_response_origin` WHERE `enabled` = 1 ORDER BY `id`"))
    {
        do
        {
            Field* fields = result->Fetch();

            ResponseOriginDefinition origin;
            origin.Id = fields[0].Get<uint32>();
            origin.Name = fields[1].Get<std::string>();
            origin.MapId = fields[2].Get<uint16>();
            origin.Team = fields[3].Get<uint8>();
            origin.MaxActiveDefault = fields[4].Get<uint32>();
            origin.Enabled = fields[5].Get<bool>();

            auto [iterator, inserted] = _responseOrigins.emplace(origin.Id, std::move(origin));
            if (!inserted)
            {
                LOG_ERROR("server.loading", "Living World: duplicate response origin id {} ignored.", iterator->first);
            }
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "Living World: loaded {} enabled response origin definition(s).", _responseOrigins.size());

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `name`, `map_id`, `zone_id`, `team`, `response_origin_id`, "
        "`recommended_min_level`, `recommended_max_level`, `selection_weight`, "
        "`minimum_cooldown_seconds`, `maximum_cooldown_seconds`, `maximum_runtime_seconds`, `allow_random_start`, `enabled` "
        "FROM `lw_invasion` WHERE `enabled` = 1 ORDER BY `id`"))
    {
        do
        {
            Field* fields = result->Fetch();

            InvasionDefinition definition;
            definition.Id = fields[0].Get<uint32>();
            definition.Name = fields[1].Get<std::string>();
            definition.MapId = fields[2].Get<uint16>();
            definition.ZoneId = fields[3].Get<uint32>();
            definition.Team = fields[4].Get<uint8>();
            definition.ResponseOriginId = fields[5].Get<uint32>();
            definition.RecommendedMinLevel = fields[6].Get<uint8>();
            definition.RecommendedMaxLevel = fields[7].Get<uint8>();
            definition.SelectionWeight = fields[8].Get<uint32>();
            definition.MinimumCooldownSeconds = fields[9].Get<uint32>();
            definition.MaximumCooldownSeconds = fields[10].Get<uint32>();
            definition.MaximumRuntimeSeconds = fields[11].Get<uint32>();
            definition.AllowRandomStart = fields[12].Get<bool>();
            definition.Enabled = fields[13].Get<bool>();

            ResponseOriginDefinition const* origin = GetResponseOrigin(definition.ResponseOriginId);
            if (!origin)
            {
                LOG_ERROR("server.loading",
                    "Living World: invasion {} ({}) references missing or disabled response origin {} and was ignored.",
                    definition.Id, definition.Name, definition.ResponseOriginId);
                continue;
            }

            if (origin->MapId != definition.MapId)
            {
                LOG_ERROR("server.loading",
                    "Living World: invasion {} ({}) uses map {}, but response origin {} ({}) uses map {}; invasion ignored.",
                    definition.Id, definition.Name, definition.MapId, origin->Id, origin->Name, origin->MapId);
                continue;
            }

            if (definition.RecommendedMinLevel > definition.RecommendedMaxLevel)
            {
                LOG_ERROR("server.loading", "Living World: invasion {} ({}) has an invalid level range and was ignored.",
                    definition.Id, definition.Name);
                continue;
            }

            if (definition.MinimumCooldownSeconds > definition.MaximumCooldownSeconds)
            {
                LOG_ERROR("server.loading", "Living World: invasion {} ({}) has minimum cooldown greater than maximum cooldown and was ignored.",
                    definition.Id, definition.Name);
                continue;
            }

            if (definition.MaximumRuntimeSeconds == 0)
            {
                LOG_ERROR("server.loading", "Living World: invasion {} ({}) has maximum runtime 0 and was ignored.",
                    definition.Id, definition.Name);
                continue;
            }

            if (definition.SelectionWeight == 0)
            {
                LOG_ERROR("server.loading", "Living World: invasion {} ({}) has selection weight 0 and was ignored.",
                    definition.Id, definition.Name);
                continue;
            }

            auto [iterator, inserted] = _definitions.emplace(definition.Id, std::move(definition));
            if (!inserted)
            {
                LOG_ERROR("server.loading", "Living World: duplicate invasion id {} ignored.", iterator->first);
            }
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "Living World: loaded {} enabled invasion definition(s).", _definitions.size());

    std::size_t stageCount = 0;
    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `invasion_id`, `stage_order`, `name`, `duration_seconds`, `completion_type`, `completion_target_id`, `enabled` "
        "FROM `lw_invasion_stage` WHERE `enabled` = 1 ORDER BY `invasion_id`, `stage_order`"))
    {
        do
        {
            Field* fields = result->Fetch();

            InvasionStageDefinition stage;
            stage.Id = fields[0].Get<uint32>();
            stage.InvasionId = fields[1].Get<uint32>();
            stage.StageOrder = fields[2].Get<uint16>();
            stage.Name = fields[3].Get<std::string>();
            stage.DurationSeconds = fields[4].Get<uint32>();
            stage.CompletionType = fields[5].Get<uint8>();
            stage.CompletionTargetId = fields[6].Get<uint32>();
            stage.Enabled = fields[7].Get<bool>();

            if (!GetDefinition(stage.InvasionId))
            {
                LOG_ERROR("server.loading",
                    "Living World: stage {} ({}) references missing or disabled invasion {} and was ignored.",
                    stage.Id, stage.Name, stage.InvasionId);
                continue;
            }

            if (stage.CompletionType > 1)
            {
                LOG_ERROR("server.loading",
                    "Living World: stage {} ({}) uses unsupported completion type {} and was ignored.",
                    stage.Id, stage.Name, stage.CompletionType);
                continue;
            }

            if (stage.CompletionType == 0 && stage.DurationSeconds == 0)
            {
                LOG_ERROR("server.loading",
                    "Living World: timer stage {} ({}) has duration 0 and was ignored.",
                    stage.Id, stage.Name);
                continue;
            }

            if (stage.CompletionType == 1 && stage.CompletionTargetId == 0)
            {
                LOG_ERROR("server.loading",
                    "Living World: signal stage {} ({}) has no completion target signal and was ignored.",
                    stage.Id, stage.Name);
                continue;
            }

            auto& stages = _stagesByInvasion[stage.InvasionId];
            bool duplicateOrder = false;
            for (InvasionStageDefinition const& existing : stages)
            {
                if (existing.StageOrder == stage.StageOrder || existing.Id == stage.Id)
                {
                    duplicateOrder = true;
                    break;
                }
            }

            if (duplicateOrder)
            {
                LOG_ERROR("server.loading",
                    "Living World: duplicate stage id/order for invasion {} ignored (stage {}).",
                    stage.InvasionId, stage.Id);
                continue;
            }

            stages.push_back(std::move(stage));
            ++stageCount;
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "Living World: loaded {} enabled stage definition(s) for {} invasion(s).",
        stageCount, _stagesByInvasion.size());

    if (QueryResult result = WorldDatabase.Query("SELECT `id`, `stage_id`, `action_order`, `action_type`, `target_id`, `parameter1`, `parameter2`, `parameter3`, `parameter4`, `delay_seconds`, `enabled` FROM `lw_stage_action` WHERE `enabled` = 1 ORDER BY `stage_id`, `action_order`"))
    {
        do
        {
            Field* fields = result->Fetch();
            StageActionDefinition action;
            action.Id = fields[0].Get<uint32>();
            action.StageId = fields[1].Get<uint32>();
            action.ActionOrder = fields[2].Get<uint16>();
            action.ActionType = fields[3].Get<uint8>();
            action.TargetId = fields[4].Get<uint32>();
            action.Parameter1 = fields[5].Get<uint32>();
            action.Parameter2 = fields[6].Get<uint32>();
            action.Parameter3 = fields[7].Get<uint32>();
            action.Parameter4 = fields[8].Get<uint32>();
            action.DelaySeconds = fields[9].Get<uint32>();
            action.Enabled = fields[10].Get<bool>();
            _actionsByStage[action.StageId].push_back(std::move(action));
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `id`, `name`, `route_node_id`, `spawn_radius`, `enabled` FROM `lw_spawn_group` WHERE `enabled` = 1"))
    {
        do
        {
            Field* fields = result->Fetch();
            SpawnGroupDefinition group;
            group.Id = fields[0].Get<uint32>();
            group.Name = fields[1].Get<std::string>();
            group.RouteNodeId = fields[2].IsNull() ? 0 : fields[2].Get<uint32>();
            group.SpawnRadius = fields[3].Get<float>();
            group.Enabled = fields[4].Get<bool>();
            _spawnGroups.emplace(group.Id, std::move(group));
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `id`, `spawn_group_id`, `entity_type`, `entity_entry`, `lw_template_id`, `count`, `level_override`, `tactical_role`, `comment` FROM `lw_spawn_member` ORDER BY `spawn_group_id`, `id`"))
    {
        do
        {
            Field* fields = result->Fetch();
            SpawnMemberDefinition member;
            member.Id = fields[0].Get<uint32>();
            member.SpawnGroupId = fields[1].Get<uint32>();
            member.EntityType = fields[2].Get<uint8>();
            member.EntityEntry = fields[3].Get<uint32>();
            if (!fields[4].IsNull())
            {
                member.LwTemplateId = fields[4].Get<uint32>();
            }
            member.Count = fields[5].Get<uint16>();
            member.LevelOverride = fields[6].Get<uint16>();

            uint8 const tacticalRole = fields[7].Get<uint8>();
            if (tacticalRole > static_cast<uint8>(TacticalRole::Support))
            {
                LOG_ERROR("server.loading",
                    "[LW Role] Spawn member {} has invalid tactical role {}; using Default.",
                    member.Id,
                    tacticalRole);
                member.Role = TacticalRole::Default;
            }
            else
            {
                member.Role = static_cast<TacticalRole>(tacticalRole);
            }

            member.Comment = fields[8].Get<std::string>();
            _spawnMembersByGroup[member.SpawnGroupId].push_back(std::move(member));
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `name`, `enabled`, `comment` "
        "FROM `lw_runtime_signal` WHERE `enabled` = 1 ORDER BY `id`"))
    {
        do
        {
            Field* fields = result->Fetch();
            RuntimeSignalDefinition signal;
            signal.Id = fields[0].Get<uint32>();
            signal.Name = fields[1].Get<std::string>();
            signal.Enabled = fields[2].Get<bool>();
            if (!fields[3].IsNull())
                signal.Comment = fields[3].Get<std::string>();

            auto [iterator, inserted] = _runtimeSignals.emplace(signal.Id, std::move(signal));
            if (!inserted)
            {
                LOG_ERROR("server.loading",
                    "[LW Signal] Duplicate runtime signal definition id {} ignored.", iterator->first);
            }
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "[LW Signal] Loaded {} runtime signal definition(s).", _runtimeSignals.size());

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `name`, `text`, `chat_type`, `language`, `enabled`, `comment` "
        "FROM `lw_dialogue` WHERE `enabled` = 1 ORDER BY `id`"))
    {
        do
        {
            Field* fields = result->Fetch();
            DialogueDefinition dialogue;
            dialogue.Id = fields[0].Get<uint32>();
            dialogue.Name = fields[1].Get<std::string>();
            dialogue.Text = fields[2].Get<std::string>();
            dialogue.ChatType = fields[3].Get<uint8>();
            dialogue.Language = fields[4].Get<uint8>();
            dialogue.Enabled = fields[5].Get<bool>();
            if (!fields[6].IsNull())
                dialogue.Comment = fields[6].Get<std::string>();

            if (dialogue.Text.empty())
            {
                LOG_ERROR("server.loading",
                    "[LW Dialogue] Dialogue {} ({}) has empty text and was ignored.",
                    dialogue.Id, dialogue.Name);
                continue;
            }

            if (dialogue.ChatType > 1)
            {
                LOG_ERROR("server.loading",
                    "[LW Dialogue] Dialogue {} ({}) uses unsupported chat type {} and was ignored.",
                    dialogue.Id, dialogue.Name, dialogue.ChatType);
                continue;
            }

            auto [iterator, inserted] = _dialogues.emplace(dialogue.Id, std::move(dialogue));
            if (!inserted)
            {
                LOG_ERROR("server.loading",
                    "[LW Dialogue] Duplicate dialogue definition id {} ignored.", iterator->first);
            }
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "[LW Dialogue] Loaded {} dialogue definition(s).", _dialogues.size());


    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `name`, `text`, `enabled`, `comment` "
        "FROM `lw_announcement` WHERE `enabled` = 1 ORDER BY `id`"))
    {
        do
        {
            Field* fields = result->Fetch();
            AnnouncementDefinition announcement;
            announcement.Id = fields[0].Get<uint32>();
            announcement.Name = fields[1].Get<std::string>();
            announcement.Text = fields[2].Get<std::string>();
            announcement.Enabled = fields[3].Get<bool>();
            if (!fields[4].IsNull())
                announcement.Comment = fields[4].Get<std::string>();

            if (announcement.Text.empty())
            {
                LOG_ERROR("server.loading",
                    "[LW Announcement] Announcement {} ({}) has empty text and was ignored.",
                    announcement.Id, announcement.Name);
                continue;
            }

            auto [iterator, inserted] = _announcements.emplace(announcement.Id, std::move(announcement));
            if (!inserted)
            {
                LOG_ERROR("server.loading",
                    "[LW Announcement] Duplicate announcement definition id {} ignored.", iterator->first);
            }
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "[LW Announcement] Loaded {} announcement definition(s).", _announcements.size());

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `name`, `enabled`, `comment` "
        "FROM `lw_movement_path` WHERE `enabled` = 1 ORDER BY `id`"))
    {
        do
        {
            Field* fields = result->Fetch();
            MovementPathDefinition path;
            path.Id = fields[0].Get<uint32>();
            path.Name = fields[1].Get<std::string>();
            path.Enabled = fields[2].Get<bool>();
            if (!fields[3].IsNull())
                path.Comment = fields[3].Get<std::string>();
            _movementPaths.emplace(path.Id, std::move(path));
        } while (result->NextRow());
    }

    std::size_t movementNodeCount = 0;
    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `path_id`, `node_order`, `map_id`, `x`, `y`, `z`, `orientation`, `wait_ms`, "
        "`profile_override_id`, `enabled`, `comment` "
        "FROM `lw_movement_node` WHERE `enabled` = 1 ORDER BY `path_id`, `node_order`"))
    {
        do
        {
            Field* fields = result->Fetch();
            MovementNodeDefinition node;
            node.Id = fields[0].Get<uint32>();
            node.PathId = fields[1].Get<uint32>();
            node.NodeOrder = fields[2].Get<uint16>();
            node.MapId = fields[3].Get<uint16>();
            node.X = fields[4].Get<float>();
            node.Y = fields[5].Get<float>();
            node.Z = fields[6].Get<float>();
            node.Orientation = fields[7].Get<float>();
            node.WaitMs = fields[8].Get<uint32>();
            node.ProfileOverrideId = fields[9].Get<uint32>();
            node.Enabled = fields[10].Get<bool>();
            if (!fields[11].IsNull())
                node.Comment = fields[11].Get<std::string>();

            if (_movementPaths.find(node.PathId) == _movementPaths.end())
            {
                LOG_ERROR("server.loading",
                    "[LW Movement] Node {} references missing or disabled movement path {}; node ignored.",
                    node.Id, node.PathId);
                continue;
            }

            _movementNodesByPath[node.PathId].push_back(std::move(node));
            ++movementNodeCount;
        } while (result->NextRow());
    }

    std::size_t movementNodeActionCount = 0;
    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `node_id`, `action_order`, `action_type`, `target_id`, `parameter1`, `parameter2`, `parameter3`, `enabled`, `comment` "
        "FROM `lw_movement_node_action` WHERE `enabled` = 1 ORDER BY `node_id`, `action_order`, `id`"))
    {
        do
        {
            Field* fields = result->Fetch();
            MovementNodeActionDefinition action;
            action.Id = fields[0].Get<uint32>();
            action.NodeId = fields[1].Get<uint32>();
            action.ActionOrder = fields[2].Get<uint16>();
            action.ActionType = fields[3].Get<uint8>();
            action.TargetId = fields[4].Get<uint32>();
            action.Parameter1 = fields[5].Get<uint32>();
            action.Parameter2 = fields[6].Get<uint32>();
            action.Parameter3 = fields[7].Get<uint32>();
            action.Enabled = fields[8].Get<bool>();
            if (!fields[9].IsNull())
                action.Comment = fields[9].Get<std::string>();

            bool nodeExists = false;
            for (auto const& [pathId, nodes] : _movementNodesByPath)
            {
                (void)pathId;
                if (std::any_of(nodes.begin(), nodes.end(), [&action](MovementNodeDefinition const& node)
                    { return node.Id == action.NodeId; }))
                {
                    nodeExists = true;
                    break;
                }
            }

            if (!nodeExists)
            {
                LOG_ERROR("server.loading",
                    "[LW Movement] Node action {} references missing or disabled movement node {}; action ignored.",
                    action.Id, action.NodeId);
                continue;
            }

            if (action.ActionType < 1 || action.ActionType > 5)
            {
                LOG_ERROR("server.loading",
                    "[LW Movement] Node action {} on node {} uses unsupported action type {}; action ignored.",
                    action.Id, action.NodeId, action.ActionType);
                continue;
            }

            _movementNodeActionsByNode[action.NodeId].push_back(std::move(action));
            ++movementNodeActionCount;
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `name`, `default_mode`, `walk_speed_multiplier`, `run_speed_multiplier`, "
        "`stealth_enabled`, `enabled`, `comment` "
        "FROM `lw_movement_profile` WHERE `enabled` = 1 ORDER BY `id`"))
    {
        do
        {
            Field* fields = result->Fetch();
            MovementProfileDefinition profile;
            profile.Id = fields[0].Get<uint32>();
            profile.Name = fields[1].Get<std::string>();
            profile.DefaultMode = fields[2].Get<uint8>();
            profile.WalkSpeedMultiplier = fields[3].Get<float>();
            profile.RunSpeedMultiplier = fields[4].Get<float>();
            profile.StealthEnabled = fields[5].Get<bool>();
            profile.Enabled = fields[6].Get<bool>();
            if (!fields[7].IsNull())
                profile.Comment = fields[7].Get<std::string>();
            _movementProfiles.emplace(profile.Id, std::move(profile));
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "[LW Movement] Loaded {} movement path(s).", _movementPaths.size());
    LOG_INFO("server.loading", "[LW Movement] Loaded {} movement node(s).", movementNodeCount);
    LOG_INFO("server.loading", "[LW Movement] Loaded {} movement node action(s).", movementNodeActionCount);
    LOG_INFO("server.loading", "[LW Movement] Loaded {} movement profile(s).", _movementProfiles.size());


    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `name`, `map_id`, `x`, `y`, `z`, `orientation`, `arrival_radius`, `enabled`, `comment` "
        "FROM `lw_route_node` WHERE `enabled` = 1 ORDER BY `id`"))
    {
        do
        {
            Field* fields = result->Fetch();

            RouteNodeDefinition node;
            node.Id = fields[0].Get<uint32>();
            node.Name = fields[1].Get<std::string>();
            node.MapId = fields[2].Get<uint16>();
            node.X = fields[3].Get<float>();
            node.Y = fields[4].Get<float>();
            node.Z = fields[5].Get<float>();
            node.Orientation = fields[6].Get<float>();
            node.ArrivalRadius = fields[7].Get<float>();
            node.Enabled = fields[8].Get<bool>();
            if (!fields[9].IsNull())
                node.Comment = fields[9].Get<std::string>();

            if (node.Name.empty())
            {
                LOG_ERROR("server.loading", "[LW Route] Route node {} has an empty name and was ignored.", node.Id);
                continue;
            }

            if (node.ArrivalRadius <= 0.0f)
            {
                LOG_ERROR("server.loading",
                    "[LW Route] Route node {} ({}) has invalid arrival radius {}; node ignored.",
                    node.Id, node.Name, node.ArrivalRadius);
                continue;
            }

            auto [iterator, inserted] = _routeNodes.emplace(node.Id, std::move(node));
            if (!inserted)
            {
                LOG_ERROR("server.loading", "[LW Route] Duplicate route node definition id {} ignored.", iterator->first);
            }
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "[LW Route] Loaded {} route node definition(s).", _routeNodes.size());

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `name`, `start_node_id`, `end_node_id`, `movement_path_id`, `enabled`, `comment` "
        "FROM `lw_route_segment` WHERE `enabled` = 1 ORDER BY `id`"))
    {
        do
        {
            Field* fields = result->Fetch();

            RouteSegmentDefinition segment;
            segment.Id = fields[0].Get<uint32>();
            segment.Name = fields[1].Get<std::string>();
            segment.StartNodeId = fields[2].Get<uint32>();
            segment.EndNodeId = fields[3].Get<uint32>();
            segment.MovementPathId = fields[4].Get<uint32>();
            segment.Enabled = fields[5].Get<bool>();
            if (!fields[6].IsNull())
                segment.Comment = fields[6].Get<std::string>();

            if (segment.Name.empty())
            {
                LOG_ERROR("server.loading", "[LW Route] Route segment {} has an empty name and was ignored.", segment.Id);
                continue;
            }

            if (segment.StartNodeId == segment.EndNodeId)
            {
                LOG_ERROR("server.loading",
                    "[LW Route] Route segment {} ({}) uses route node {} as both its start and end; segment ignored.",
                    segment.Id, segment.Name, segment.StartNodeId);
                continue;
            }

            if (_routeNodes.find(segment.StartNodeId) == _routeNodes.end())
            {
                LOG_ERROR("server.loading",
                    "[LW Route] Route segment {} ({}) references missing or disabled start route node {}; segment ignored.",
                    segment.Id, segment.Name, segment.StartNodeId);
                continue;
            }

            if (_routeNodes.find(segment.EndNodeId) == _routeNodes.end())
            {
                LOG_ERROR("server.loading",
                    "[LW Route] Route segment {} ({}) references missing or disabled end route node {}; segment ignored.",
                    segment.Id, segment.Name, segment.EndNodeId);
                continue;
            }

            if (_movementPaths.find(segment.MovementPathId) == _movementPaths.end())
            {
                LOG_ERROR("server.loading",
                    "[LW Route] Route segment {} ({}) references missing or disabled movement path {}; segment ignored.",
                    segment.Id, segment.Name, segment.MovementPathId);
                continue;
            }

            auto [iterator, inserted] = _routeSegments.emplace(segment.Id, std::move(segment));
            if (!inserted)
            {
                LOG_ERROR("server.loading", "[LW Route] Duplicate route segment definition id {} ignored.", iterator->first);
            }
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "[LW Route] Loaded {} route segment definition(s).", _routeSegments.size());

    std::size_t routeNodeActionCount = 0;
    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `invasion_id`, `spawn_group_id`, `route_node_id`, `action_order`, `action_type`, "
        "`target_id`, `parameter1`, `parameter2`, `parameter3`, `enabled`, `comment` "
        "FROM `lw_route_node_action` WHERE `enabled` = 1 "
        "ORDER BY `invasion_id`, `spawn_group_id`, `route_node_id`, `action_order`, `id`"))
    {
        do
        {
            Field* fields = result->Fetch();
            RouteNodeActionDefinition action;
            action.Id = fields[0].Get<uint32>();
            action.InvasionId = fields[1].Get<uint32>();
            action.SpawnGroupId = fields[2].Get<uint32>();
            action.RouteNodeId = fields[3].Get<uint32>();
            action.ActionOrder = fields[4].Get<uint16>();
            action.ActionType = fields[5].Get<uint8>();
            action.TargetId = fields[6].Get<uint32>();
            action.Parameter1 = fields[7].Get<uint32>();
            action.Parameter2 = fields[8].Get<uint32>();
            action.Parameter3 = fields[9].Get<uint32>();
            action.Enabled = fields[10].Get<bool>();
            if (!fields[11].IsNull())
                action.Comment = fields[11].Get<std::string>();

            if (_definitions.find(action.InvasionId) == _definitions.end())
            {
                LOG_ERROR("server.loading",
                    "[LW Route] Route node action {} references missing invasion {}; action ignored.",
                    action.Id, action.InvasionId);
                continue;
            }

            if (_spawnGroups.find(action.SpawnGroupId) == _spawnGroups.end())
            {
                LOG_ERROR("server.loading",
                    "[LW Route] Route node action {} references missing spawn group {}; action ignored.",
                    action.Id, action.SpawnGroupId);
                continue;
            }

            if (_routeNodes.find(action.RouteNodeId) == _routeNodes.end())
            {
                LOG_ERROR("server.loading",
                    "[LW Route] Route node action {} references missing route node {}; action ignored.",
                    action.Id, action.RouteNodeId);
                continue;
            }

            if (action.ActionType < 1 || action.ActionType > 3)
            {
                LOG_ERROR("server.loading",
                    "[LW Route] Route node action {} uses unsupported action type {}; action ignored.",
                    action.Id, action.ActionType);
                continue;
            }

            uint64 const key = (static_cast<uint64>(action.InvasionId) << 32) | action.SpawnGroupId;
            _routeNodeActionsByInvasionGroup[key].push_back(std::move(action));
            ++routeNodeActionCount;
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "[LW Route] Loaded {} route node action definition(s).", routeNodeActionCount);
}

ResponseOriginDefinition const* LivingWorldDataMgr::GetResponseOrigin(uint32 responseOriginId) const
{
    auto const iterator = _responseOrigins.find(responseOriginId);
    return iterator != _responseOrigins.end() ? &iterator->second : nullptr;
}

InvasionDefinition const* LivingWorldDataMgr::GetDefinition(uint32 invasionId) const
{
    auto const iterator = _definitions.find(invasionId);
    return iterator != _definitions.end() ? &iterator->second : nullptr;
}

std::unordered_map<uint32, ResponseOriginDefinition> const& LivingWorldDataMgr::GetResponseOrigins() const
{
    return _responseOrigins;
}

std::unordered_map<uint32, InvasionDefinition> const& LivingWorldDataMgr::GetDefinitions() const
{
    return _definitions;
}

std::vector<InvasionStageDefinition> const* LivingWorldDataMgr::GetStages(uint32 invasionId) const
{
    auto const iterator = _stagesByInvasion.find(invasionId);
    return iterator != _stagesByInvasion.end() ? &iterator->second : nullptr;
}

std::size_t LivingWorldDataMgr::GetDefinitionCount() const
{
    return _definitions.size();
}
}


namespace lw
{
std::vector<StageActionDefinition> const* LivingWorldDataMgr::GetActions(uint32 stageId) const
{
    auto it = _actionsByStage.find(stageId);
    return it != _actionsByStage.end() ? &it->second : nullptr;
}
SpawnGroupDefinition const* LivingWorldDataMgr::GetSpawnGroup(uint32 id) const
{
    auto it = _spawnGroups.find(id);
    return it != _spawnGroups.end() ? &it->second : nullptr;
}
std::vector<SpawnMemberDefinition> const* LivingWorldDataMgr::GetSpawnMembers(uint32 id) const
{
    auto it = _spawnMembersByGroup.find(id);
    return it != _spawnMembersByGroup.end() ? &it->second : nullptr;
}

MovementPathDefinition const* LivingWorldDataMgr::GetMovementPath(uint32 id) const
{
    auto it = _movementPaths.find(id);
    return it != _movementPaths.end() ? &it->second : nullptr;
}

std::vector<MovementNodeDefinition> const* LivingWorldDataMgr::GetMovementNodes(uint32 pathId) const
{
    auto it = _movementNodesByPath.find(pathId);
    return it != _movementNodesByPath.end() ? &it->second : nullptr;
}

std::vector<MovementNodeActionDefinition> const* LivingWorldDataMgr::GetMovementNodeActions(uint32 nodeId) const
{
    auto it = _movementNodeActionsByNode.find(nodeId);
    return it != _movementNodeActionsByNode.end() ? &it->second : nullptr;
}

MovementProfileDefinition const* LivingWorldDataMgr::GetMovementProfile(uint32 id) const
{
    auto it = _movementProfiles.find(id);
    return it != _movementProfiles.end() ? &it->second : nullptr;
}

RouteNodeDefinition const* LivingWorldDataMgr::GetRouteNode(uint32 id) const
{
    auto const iterator = _routeNodes.find(id);
    return iterator != _routeNodes.end() ? &iterator->second : nullptr;
}

RouteNodeDefinition const* LivingWorldDataMgr::GetRouteNode(std::string const& name) const
{
    for (auto const& [id, node] : _routeNodes)
    {
        (void)id;
        if (node.Name == name)
            return &node;
    }

    return nullptr;
}

RouteSegmentDefinition const* LivingWorldDataMgr::GetRouteSegment(uint32 id) const
{
    auto const iterator = _routeSegments.find(id);
    return iterator != _routeSegments.end() ? &iterator->second : nullptr;
}

RouteSegmentDefinition const* LivingWorldDataMgr::GetRouteSegment(std::string const& name) const
{
    for (auto const& [id, segment] : _routeSegments)
    {
        (void)id;
        if (segment.Name == name)
            return &segment;
    }

    return nullptr;
}

std::unordered_map<uint32, RouteSegmentDefinition> const& LivingWorldDataMgr::GetRouteSegments() const
{
    return _routeSegments;
}

std::vector<RouteNodeActionDefinition> const* LivingWorldDataMgr::GetRouteNodeActions(uint32 invasionId, uint32 spawnGroupId) const
{
    uint64 const key = (static_cast<uint64>(invasionId) << 32) | spawnGroupId;
    auto const iterator = _routeNodeActionsByInvasionGroup.find(key);
    return iterator != _routeNodeActionsByInvasionGroup.end() ? &iterator->second : nullptr;
}

RuntimeSignalDefinition const* LivingWorldDataMgr::GetRuntimeSignal(uint32 id) const
{
    auto it = _runtimeSignals.find(id);
    return it != _runtimeSignals.end() ? &it->second : nullptr;
}

DialogueDefinition const* LivingWorldDataMgr::GetDialogue(uint32 id) const
{
    auto it = _dialogues.find(id);
    return it != _dialogues.end() ? &it->second : nullptr;
}

AnnouncementDefinition const* LivingWorldDataMgr::GetAnnouncement(uint32 id) const
{
    auto it = _announcements.find(id);
    return it != _announcements.end() ? &it->second : nullptr;
}

std::size_t LivingWorldDataMgr::GetResponseOriginCount() const
{
    return _responseOrigins.size();
}

std::size_t LivingWorldDataMgr::GetStageCount() const
{
    std::size_t count = 0;
    for (auto const& [invasionId, stages] : _stagesByInvasion)
    {
        (void)invasionId;
        count += stages.size();
    }
    return count;
}

std::size_t LivingWorldDataMgr::GetActionCount() const
{
    std::size_t count = 0;
    for (auto const& [stageId, actions] : _actionsByStage)
    {
        (void)stageId;
        count += actions.size();
    }
    return count;
}

std::size_t LivingWorldDataMgr::GetSpawnGroupCount() const
{
    return _spawnGroups.size();
}

std::size_t LivingWorldDataMgr::GetSpawnMemberCount() const
{
    std::size_t count = 0;
    for (auto const& [groupId, members] : _spawnMembersByGroup)
    {
        (void)groupId;
        count += members.size();
    }
    return count;
}

std::size_t LivingWorldDataMgr::GetMovementPathCount() const
{
    return _movementPaths.size();
}

std::size_t LivingWorldDataMgr::GetMovementNodeCount() const
{
    std::size_t count = 0;
    for (auto const& [pathId, nodes] : _movementNodesByPath)
    {
        (void)pathId;
        count += nodes.size();
    }
    return count;
}

std::size_t LivingWorldDataMgr::GetMovementNodeActionCount() const
{
    std::size_t count = 0;
    for (auto const& [nodeId, actions] : _movementNodeActionsByNode)
    {
        (void)nodeId;
        count += actions.size();
    }
    return count;
}

std::size_t LivingWorldDataMgr::GetMovementProfileCount() const
{
    return _movementProfiles.size();
}

std::size_t LivingWorldDataMgr::GetRouteNodeCount() const
{
    return _routeNodes.size();
}

std::size_t LivingWorldDataMgr::GetRouteSegmentCount() const
{
    return _routeSegments.size();
}

std::size_t LivingWorldDataMgr::GetRouteNodeActionCount() const
{
    std::size_t count = 0;
    for (auto const& [key, actions] : _routeNodeActionsByInvasionGroup)
    {
        (void)key;
        count += actions.size();
    }
    return count;
}

std::size_t LivingWorldDataMgr::GetRuntimeSignalCount() const
{
    return _runtimeSignals.size();
}

std::size_t LivingWorldDataMgr::GetDialogueCount() const
{
    return _dialogues.size();
}

std::size_t LivingWorldDataMgr::GetAnnouncementCount() const
{
    return _announcements.size();
}
}
