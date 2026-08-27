#include "MovementController.h"

#include "Creature.h"
#include "GameObject.h"
#include "AnnouncementManager.h"
#include "DialogueManager.h"
#include "IEntityProvider.h"
#include "LivingWorld.h"
#include "InvasionRuntimeManager.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "RuntimeEntityGroup.h"
#include "RuntimeSignalManager.h"
#include "SoundManager.h"
#include "Timer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lw
{
namespace
{
constexpr uint32 MovementUpdateIntervalMs = 250;
constexpr float ArrivalTolerance = 2.0f;
constexpr float FormationArrivalTolerance = 10.0f;
constexpr uint32 FormationArrivalPercent = 75;
constexpr uint32 FormationArrivalGraceMs = 3000;
constexpr float FinalObjectiveArrivalRadius = 20.0f;
constexpr float RouteCatchupDistance = 12.0f;
constexpr float RouteOffRoadRecoveryDistance = 10.0f;
constexpr float RouteBreadcrumbTolerance = 2.5f;
constexpr uint32 RouteCatchupRefreshMs = 1500;
constexpr uint32 RouteCatchupWatchdogMs = 30000;
constexpr float RouteCatchupEndpointTolerance = 2.0f;
constexpr std::size_t RouteRecoveryCandidateLimit = 24;
constexpr float Pi = 3.14159265358979323846f;

struct FormationOffset
{
    float Forward = 0.0f;
    float Right = 0.0f;
};

FormationOffset GetFormationOffset(uint8 tacticalRole, uint32 roleSlot)
{
    TacticalRole const role = static_cast<TacticalRole>(tacticalRole);
    float const side = (roleSlot % 2U == 0U ? -1.0f : 1.0f);
    float const rank = static_cast<float>(roleSlot / 2U);

    switch (role)
    {
        case TacticalRole::Commander:
            return { 0.0f, side * rank * 1.5f };
        case TacticalRole::Protector:
            return { 2.5f + rank, side * (1.5f + rank * 0.75f) };
        case TacticalRole::MeleeDps:
            return { 4.0f + rank, side * (2.0f + rank * 1.25f) };
        case TacticalRole::RangedDps:
            return { -4.0f - rank, side * (2.5f + rank * 1.5f) };
        case TacticalRole::Healer:
            return { -6.0f - rank, side * (1.5f + rank) };
        case TacticalRole::Support:
            return { -5.0f - rank, side * (4.0f + rank) };
        case TacticalRole::Default:
        default:
            return { 0.0f, 0.0f };
    }
}

// Route journeys use a deliberately compact marching column rather than the
// role-aware combat/staging formation.  The route recorder already describes
// the safe road centerline every ~5 yards, so route travel should keep the
// entire force close to that centerline instead of allowing role slots to grow
// outward into a large X.  Slot 0 is the route leader at the exact node center.
FormationOffset GetCaravanFormationOffset(uint32 marchSlot)
{
    // Slot 0 is the merchant/route owner.  The first two followers are pack
    // animals in a shallow V behind him instead of the generic five-wide road
    // column.  Additional followers, if added later, continue in paired ranks.
    if (marchSlot == 0)
        return { 0.0f, 0.0f };

    uint32 const followerSlot = marchSlot - 1;
    uint32 const rank = followerSlot / 2;
    bool const left = (followerSlot % 2U) == 0U;

    float const forward = -5.0f - static_cast<float>(rank) * 2.25f;
    float const right = (left ? -1.35f : 1.35f);
    return { forward, right };
}

FormationOffset GetMarchFormationOffset(uint32 marchSlot)
{
    if (marchSlot == 0)
        return { 0.0f, 0.0f };

    constexpr uint32 Columns = 5;
    constexpr float ColumnSpacing = 1.25f;
    constexpr float RowSpacing = 1.50f;

    uint32 const followerSlot = marchSlot - 1;
    uint32 const row = followerSlot / Columns;
    uint32 const column = followerSlot % Columns;

    // Five narrow lanes: -2.5, -1.25, 0, +1.25, +2.5 yards.
    float const centeredColumn = static_cast<float>(column) - 2.0f;
    float const right = centeredColumn * ColumnSpacing;

    // Rows trail the route leader.  Keeping the lateral footprint narrow is
    // more important than preserving tactical-role spacing while on roads.
    float const forward = -RowSpacing * static_cast<float>(row + 1);
    return { forward, right };
}

void BuildMarchDestination(
    MovementNodeDefinition const& node,
    MovementDirection direction,
    uint32 marchSlot,
    RouteFormationProfile formationProfile,
    float& x,
    float& y,
    float& z)
{
    FormationOffset const offset = formationProfile == RouteFormationProfile::TravelingCaravan
        ? GetCaravanFormationOffset(marchSlot)
        : GetMarchFormationOffset(marchSlot);

    float const travelOrientation = node.Orientation +
        (direction == MovementDirection::Reverse ? Pi : 0.0f);
    float const forwardX = std::cos(travelOrientation);
    float const forwardY = std::sin(travelOrientation);
    float const rightX = -forwardY;
    float const rightY = forwardX;

    x = node.X + forwardX * offset.Forward + rightX * offset.Right;
    y = node.Y + forwardY * offset.Forward + rightY * offset.Right;
    z = node.Z;
}

void BuildFormationDestination(
    MovementNodeDefinition const& node,
    MovementDirection direction,
    uint8 tacticalRole,
    uint32 roleSlot,
    float& x,
    float& y,
    float& z)
{
    FormationOffset const offset = GetFormationOffset(tacticalRole, roleSlot);

    float const travelOrientation = node.Orientation +
        (direction == MovementDirection::Reverse ? Pi : 0.0f);
    float const forwardX = std::cos(travelOrientation);
    float const forwardY = std::sin(travelOrientation);
    float const rightX = -forwardY;
    float const rightY = forwardX;

    x = node.X + forwardX * offset.Forward + rightX * offset.Right;
    y = node.Y + forwardY * offset.Forward + rightY * offset.Right;
    z = node.Z;
}

uint32 ExecuteNodeActions(ActiveRuntimeMovement const& movement, RuntimeEntityGroup const& group, MovementNodeDefinition const& node)
{
    auto const* actions = sLivingWorldDataMgr.GetMovementNodeActions(node.Id);
    if (!actions || actions->empty())
    {
        return 0;
    }

    uint32 requestedWaitMs = 0;

    InvasionRuntime const* runtime = sInvasionRuntimeMgr.GetRuntime(movement.RuntimeId);

    for (MovementNodeActionDefinition const& action : *actions)
    {
        bool success = false;

        switch (action.ActionType)
        {
            case 1: // Dialogue: target_id=dialogue, parameter1=speaker member id.
                success = sDialogueManager.Execute(
                    movement.RuntimeId,
                    group.SpawnGroupId,
                    action.TargetId,
                    action.Parameter1);
                break;

            case 2: // Announcement: target_id=announcement, p1=scope, p2=scope id, p3=faction.
                if (!runtime)
                {
                    LOG_ERROR("server.loading",
                        "[LW Movement] Runtime entity group #{} cannot execute announcement node action {} because runtime #{} was not found.",
                        movement.RuntimeGroupId, action.Id, movement.RuntimeId);
                    continue;
                }

                success = sAnnouncementManager.Execute(
                    movement.RuntimeId,
                    runtime->GetInvasionId(),
                    action.TargetId,
                    action.Parameter1,
                    action.Parameter2,
                    action.Parameter3);
                break;

            case 3: // Sound: target_id=sound, parameter1=source member id, parameter2=playback mode.
                success = sSoundManager.Execute(
                    movement.RuntimeId,
                    group.SpawnGroupId,
                    action.TargetId,
                    action.Parameter1,
                    action.Parameter2);
                break;

            case 4: // Open GameObject by DB spawn GUID. parameter1 = optional hold before movement continues (ms).
            {
                GameObject* gameObject =
                    ObjectAccessor::GetSpawnedGameObjectByDBGUID(node.MapId, action.TargetId);

                if (!gameObject)
                {
                    LOG_ERROR("server.loading",
                        "[LW Movement] Runtime entity group #{} could not find GameObject DB GUID {} "
                        "for OPEN_GAMEOBJECT node action {} on map {}.",
                        movement.RuntimeGroupId,
                        action.TargetId,
                        action.Id,
                        node.MapId);
                    break;
                }

                if (gameObject->GetGoType() != GAMEOBJECT_TYPE_DOOR &&
                    gameObject->GetGoType() != GAMEOBJECT_TYPE_BUTTON)
                {
                    LOG_ERROR("server.loading",
                        "[LW Movement] Runtime entity group #{} node action {} tried to open "
                        "GameObject DB GUID {} entry {}, but its type {} is not a door/button.",
                        movement.RuntimeGroupId,
                        action.Id,
                        action.TargetId,
                        gameObject->GetEntry(),
                        static_cast<uint32>(gameObject->GetGoType()));
                    break;
                }

                // This is a server-script operation, not a player interaction.
                // Player lock/key requirements remain unchanged.
                if (gameObject->getLootState() == GO_READY)
                    gameObject->UseDoorOrButton();

                requestedWaitMs = std::max(requestedWaitMs, action.Parameter1);
                success = true;
                break;
            }

            case 5: // Close/reset GameObject by DB spawn GUID. parameter1 = optional hold before movement continues (ms).
            {
                GameObject* gameObject =
                    ObjectAccessor::GetSpawnedGameObjectByDBGUID(node.MapId, action.TargetId);

                if (!gameObject)
                {
                    LOG_ERROR("server.loading",
                        "[LW Movement] Runtime entity group #{} could not find GameObject DB GUID {} "
                        "for CLOSE_GAMEOBJECT node action {} on map {}.",
                        movement.RuntimeGroupId,
                        action.TargetId,
                        action.Id,
                        node.MapId);
                    break;
                }

                if (gameObject->GetGoType() != GAMEOBJECT_TYPE_DOOR &&
                    gameObject->GetGoType() != GAMEOBJECT_TYPE_BUTTON)
                {
                    LOG_ERROR("server.loading",
                        "[LW Movement] Runtime entity group #{} node action {} tried to close "
                        "GameObject DB GUID {} entry {}, but its type {} is not a door/button.",
                        movement.RuntimeGroupId,
                        action.Id,
                        action.TargetId,
                        gameObject->GetEntry(),
                        static_cast<uint32>(gameObject->GetGoType()));
                    break;
                }

                if (gameObject->getLootState() == GO_ACTIVATED)
                    gameObject->ResetDoorOrButton();

                requestedWaitMs = std::max(requestedWaitMs, action.Parameter1);
                success = true;
                break;
            }

            default:
                LOG_ERROR("server.loading",
                    "[LW Movement] Runtime entity group #{} encountered unsupported node action type {} for action {}.",
                    movement.RuntimeGroupId, action.ActionType, action.Id);
                continue;
        }

        if (!success)
        {
            LOG_ERROR("server.loading",
                "[LW Movement] Runtime entity group #{} failed node action {} type {} at path {} node {}.",
                movement.RuntimeGroupId, action.Id, action.ActionType, movement.PathId, node.NodeOrder);
            continue;
        }

        LOG_INFO("server.loading",
            "[LW Movement] Runtime entity group #{} executed node action {} type {} at path {} node {}.",
            movement.RuntimeGroupId, action.Id, action.ActionType, movement.PathId, node.NodeOrder);
    }

    return requestedWaitMs;
}
}

MovementController& MovementController::Instance()
{
    static MovementController instance;
    return instance;
}

void MovementController::Reset()
{
    _activeMovements.clear();
    _activeRouteJourneys.clear();
    _triggeredRouteActionIdsByGroup.clear();
    _updateTimerMs = 0;
}

bool MovementController::StartPath(
    uint64 runtimeGroupId,
    uint32 pathId,
    uint32 profileId,
    uint32 completionSignalId,
    MovementDirection direction,
    bool routeMovement)
{
    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(runtimeGroupId);
    if (!group)
    {
        LOG_ERROR("server.loading",
            "[LW Movement] Cannot start path {} because runtime entity group #{} does not exist.",
            pathId, runtimeGroupId);
        return false;
    }

    MovementPathDefinition const* path = sLivingWorldDataMgr.GetMovementPath(pathId);
    if (!path)
    {
        LOG_ERROR("server.loading",
            "[LW Movement] Runtime entity group #{} requested missing movement path {}.",
            runtimeGroupId, pathId);
        return false;
    }

    auto const* nodes = sLivingWorldDataMgr.GetMovementNodes(pathId);
    if (!nodes || nodes->empty())
    {
        LOG_ERROR("server.loading",
            "[LW Movement] Movement path {} ({}) has no enabled nodes.",
            pathId, path->Name);
        return false;
    }

    if (profileId != 0 && !sLivingWorldDataMgr.GetMovementProfile(profileId))
    {
        LOG_ERROR("server.loading",
            "[LW Movement] Runtime entity group #{} requested missing movement profile {}.",
            runtimeGroupId, profileId);
        return false;
    }

    if (completionSignalId != 0 && !sLivingWorldDataMgr.GetRuntimeSignal(completionSignalId))
    {
        LOG_ERROR("server.loading",
            "[LW Movement] Runtime entity group #{} requested missing completion signal {}.",
            runtimeGroupId, completionSignalId);
        return false;
    }

    ActiveRuntimeMovement movement;
    movement.RuntimeGroupId = runtimeGroupId;
    movement.RuntimeId = group->RuntimeId;
    movement.PathId = pathId;
    movement.ProfileId = profileId;
    movement.CompletionSignalId = completionSignalId;
    movement.Direction = direction;
    movement.RouteMovement = routeMovement;
    movement.NodeIndex = direction == MovementDirection::Reverse ? nodes->size() - 1 : 0;
    movement.State = RuntimeMovementState::Moving;

    _activeMovements[runtimeGroupId] = movement;

    ActiveRuntimeMovement& active = _activeMovements[runtimeGroupId];
    if (!BeginCurrentNode(active))
    {
        _activeMovements.erase(runtimeGroupId);
        return false;
    }

    LOG_INFO("server.loading",
        "[LW Movement] Runtime entity group #{} started path {} ({}) {} with {} node(s){}.",
        runtimeGroupId,
        pathId,
        path->Name,
        direction == MovementDirection::Reverse ? "in reverse" : "forward",
        nodes->size(),
        profileId != 0 ? " using a movement profile" : "");

    return true;
}

bool MovementController::StartRouteSegment(
    uint64 runtimeGroupId,
    uint32 routeSegmentId,
    uint32 fromNodeId,
    uint32 profileId,
    uint32 completionSignalId)
{
    RouteSegmentDefinition const* segment = sLivingWorldDataMgr.GetRouteSegment(routeSegmentId);
    if (!segment)
    {
        LOG_ERROR("server.loading",
            "[LW Route] Runtime entity group #{} requested missing route segment {}.",
            runtimeGroupId, routeSegmentId);
        return false;
    }

    MovementDirection direction = MovementDirection::Forward;
    uint32 destinationNodeId = 0;

    if (fromNodeId == segment->StartNodeId)
    {
        direction = MovementDirection::Forward;
        destinationNodeId = segment->EndNodeId;
    }
    else if (fromNodeId == segment->EndNodeId)
    {
        direction = MovementDirection::Reverse;
        destinationNodeId = segment->StartNodeId;
    }
    else
    {
        LOG_ERROR("server.loading",
            "[LW Route] Runtime entity group #{} cannot start route segment {} ({}) from route node {} because the segment connects nodes {} and {}.",
            runtimeGroupId,
            segment->Id,
            segment->Name,
            fromNodeId,
            segment->StartNodeId,
            segment->EndNodeId);
        return false;
    }

    RouteNodeDefinition const* fromNode = sLivingWorldDataMgr.GetRouteNode(fromNodeId);
    RouteNodeDefinition const* destinationNode = sLivingWorldDataMgr.GetRouteNode(destinationNodeId);
    if (!fromNode || !destinationNode)
    {
        LOG_ERROR("server.loading",
            "[LW Route] Route segment {} ({}) references unavailable route node data; movement was not started.",
            segment->Id, segment->Name);
        return false;
    }

    if (!StartPath(
            runtimeGroupId,
            segment->MovementPathId,
            profileId,
            completionSignalId,
            direction,
            true))
    {
        LOG_ERROR("server.loading",
            "[LW Route] Runtime entity group #{} failed to start route segment {} ({}) from {} to {} using movement path {}.",
            runtimeGroupId,
            segment->Id,
            segment->Name,
            fromNode->Name,
            destinationNode->Name,
            segment->MovementPathId);
        return false;
    }

    LOG_INFO("server.loading",
        "[LW Route] Runtime entity group #{} started route segment {} ({}) from {} to {} using movement path {} {}.",
        runtimeGroupId,
        segment->Id,
        segment->Name,
        fromNode->Name,
        destinationNode->Name,
        segment->MovementPathId,
        direction == MovementDirection::Reverse ? "in reverse" : "forward");

    return true;
}

bool MovementController::BuildRouteJourney(
    uint32 fromNodeId,
    uint32 destinationNodeId,
    std::vector<RouteJourneyStep>& steps) const
{
    steps.clear();

    if (fromNodeId == destinationNodeId)
    {
        return false;
    }

    if (!sLivingWorldDataMgr.GetRouteNode(fromNodeId) || !sLivingWorldDataMgr.GetRouteNode(destinationNodeId))
    {
        return false;
    }

    struct PreviousHop
    {
        uint32 PreviousNodeId = 0;
        uint32 SegmentId = 0;
    };

    std::queue<uint32> pending;
    std::unordered_map<uint32, PreviousHop> previous;

    previous.emplace(fromNodeId, PreviousHop{});
    pending.push(fromNodeId);

    while (!pending.empty())
    {
        uint32 const currentNodeId = pending.front();
        pending.pop();

        if (currentNodeId == destinationNodeId)
        {
            break;
        }

        for (auto const& [segmentId, segment] : sLivingWorldDataMgr.GetRouteSegments())
        {
            uint32 nextNodeId = 0;

            if (segment.StartNodeId == currentNodeId)
            {
                nextNodeId = segment.EndNodeId;
            }
            else if (segment.EndNodeId == currentNodeId)
            {
                nextNodeId = segment.StartNodeId;
            }
            else
            {
                continue;
            }

            if (previous.find(nextNodeId) != previous.end())
            {
                continue;
            }

            previous.emplace(nextNodeId, PreviousHop{ currentNodeId, segmentId });
            pending.push(nextNodeId);
        }
    }

    if (previous.find(destinationNodeId) == previous.end())
    {
        return false;
    }

    uint32 currentNodeId = destinationNodeId;
    while (currentNodeId != fromNodeId)
    {
        auto const previousItr = previous.find(currentNodeId);
        if (previousItr == previous.end() || previousItr->second.SegmentId == 0)
        {
            steps.clear();
            return false;
        }

        RouteJourneyStep step;
        step.SegmentId = previousItr->second.SegmentId;
        step.FromNodeId = previousItr->second.PreviousNodeId;
        step.ToNodeId = currentNodeId;
        steps.push_back(step);

        currentNodeId = previousItr->second.PreviousNodeId;
    }

    std::reverse(steps.begin(), steps.end());
    return !steps.empty();
}

bool MovementController::StartRouteJourney(
    uint64 runtimeGroupId,
    uint32 fromNodeId,
    uint32 destinationNodeId,
    uint32 profileId,
    uint32 completionSignalId)
{
    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(runtimeGroupId);
    if (!group)
    {
        LOG_ERROR("server.loading",
            "[LW Route] Cannot start route journey because runtime entity group #{} does not exist.",
            runtimeGroupId);
        return false;
    }

    RouteNodeDefinition const* fromNode = sLivingWorldDataMgr.GetRouteNode(fromNodeId);
    RouteNodeDefinition const* destinationNode = sLivingWorldDataMgr.GetRouteNode(destinationNodeId);
    if (!fromNode || !destinationNode)
    {
        LOG_ERROR("server.loading",
            "[LW Route] Runtime entity group #{} requested a route journey using unavailable node(s) {} -> {}.",
            runtimeGroupId, fromNodeId, destinationNodeId);
        return false;
    }

    if (profileId != 0 && !sLivingWorldDataMgr.GetMovementProfile(profileId))
    {
        LOG_ERROR("server.loading",
            "[LW Route] Runtime entity group #{} requested missing movement profile {} for route journey.",
            runtimeGroupId, profileId);
        return false;
    }

    if (completionSignalId != 0 && !sLivingWorldDataMgr.GetRuntimeSignal(completionSignalId))
    {
        LOG_ERROR("server.loading",
            "[LW Route] Runtime entity group #{} requested missing completion signal {} for route journey.",
            runtimeGroupId, completionSignalId);
        return false;
    }

    std::vector<RouteJourneyStep> steps;
    if (!BuildRouteJourney(fromNodeId, destinationNodeId, steps))
    {
        LOG_ERROR("server.loading",
            "[LW Route] No connected route exists from node {} ({}) to node {} ({}).",
            fromNodeId, fromNode->Name, destinationNodeId, destinationNode->Name);
        return false;
    }

    ActiveRouteJourney journey;
    journey.RuntimeGroupId = runtimeGroupId;
    journey.RuntimeId = group->RuntimeId;
    journey.StartNodeId = fromNodeId;
    journey.DestinationNodeId = destinationNodeId;
    journey.ProfileId = profileId;
    journey.CompletionSignalId = completionSignalId;
    journey.StepIndex = 0;
    journey.Steps = std::move(steps);

    _activeRouteJourneys[runtimeGroupId] = std::move(journey);
    NotifyRouteNodeReached(runtimeGroupId, fromNodeId);

    ActiveRouteJourney const& activeJourney = _activeRouteJourneys[runtimeGroupId];
    RouteJourneyStep const& firstStep = activeJourney.Steps.front();
    uint32 const firstCompletionSignalId = activeJourney.Steps.size() == 1
        ? activeJourney.CompletionSignalId
        : 0;

    if (!StartRouteSegment(
            runtimeGroupId,
            firstStep.SegmentId,
            firstStep.FromNodeId,
            activeJourney.ProfileId,
            firstCompletionSignalId))
    {
        _activeRouteJourneys.erase(runtimeGroupId);
        return false;
    }

    LOG_INFO("server.loading",
        "[LW Route] Runtime entity group #{} started route journey from {} ({}) to {} ({}) across {} segment(s).",
        runtimeGroupId,
        fromNode->Name,
        fromNodeId,
        destinationNode->Name,
        destinationNodeId,
        activeJourney.Steps.size());

    return true;
}

bool MovementController::CancelGroup(uint64 runtimeGroupId)
{
    auto itr = _activeMovements.find(runtimeGroupId);
    if (itr == _activeMovements.end())
    {
        return false;
    }

    _activeMovements.erase(itr);
    _activeRouteJourneys.erase(runtimeGroupId);
    _triggeredRouteActionIdsByGroup.erase(runtimeGroupId);

    LOG_INFO("server.loading",
        "[LW Movement] Cancelled movement for runtime entity group #{}.",
        runtimeGroupId);
    return true;
}

void MovementController::CancelRuntime(uint64 runtimeId)
{
    std::vector<uint64> cancelled;

    for (auto const& [runtimeGroupId, movement] : _activeMovements)
    {
        if (movement.RuntimeId == runtimeId)
        {
            cancelled.push_back(runtimeGroupId);
        }
    }

    for (uint64 runtimeGroupId : cancelled)
    {
        _activeMovements.erase(runtimeGroupId);
        _activeRouteJourneys.erase(runtimeGroupId);
        _triggeredRouteActionIdsByGroup.erase(runtimeGroupId);
    }

    for (auto itr = _triggeredRouteActionIdsByGroup.begin(); itr != _triggeredRouteActionIdsByGroup.end();)
    {
        RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(itr->first);
        if (!group || group->RuntimeId == runtimeId)
            itr = _triggeredRouteActionIdsByGroup.erase(itr);
        else
            ++itr;
    }

    if (!cancelled.empty())
    {
        LOG_INFO("server.loading",
            "[LW Movement] Cancelled {} active movement(s) for runtime #{}.",
            cancelled.size(), runtimeId);
    }
}

void MovementController::NotifyRouteNodeReached(uint64 runtimeGroupId, uint32 routeNodeId, uint32 invasionId)
{
    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(runtimeGroupId);
    if (!group || group->State != RuntimeEntityGroupState::Active)
        return;

    if (invasionId == 0)
    {
        InvasionRuntime const* runtime = sInvasionRuntimeMgr.GetRuntime(group->RuntimeId);
        if (!runtime)
            return;
        invasionId = runtime->GetInvasionId();
    }

    auto const* actions = sLivingWorldDataMgr.GetRouteNodeActions(invasionId, group->SpawnGroupId);
    if (!actions)
        return;

    auto& triggered = _triggeredRouteActionIdsByGroup[runtimeGroupId];
    for (RouteNodeActionDefinition const& action : *actions)
    {
        if (action.RouteNodeId != routeNodeId || triggered.find(action.Id) != triggered.end())
            continue;

        if (ExecuteRouteNodeAction(*group, action))
            triggered.insert(action.Id);
    }
}

void MovementController::CheckRouteNodeActions(RuntimeEntityGroup const& group)
{
    InvasionRuntime const* runtime = sInvasionRuntimeMgr.GetRuntime(group.RuntimeId);
    if (!runtime)
        return;

    auto const* actions = sLivingWorldDataMgr.GetRouteNodeActions(runtime->GetInvasionId(), group.SpawnGroupId);
    if (!actions || actions->empty())
        return;

    Creature* anchor = nullptr;
    for (RuntimeEntity const& entity : group.Entities)
    {
        if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
            continue;

        Map* map = sMapMgr->FindMap(entity.MapId, 0);
        if (!map)
            continue;

        Creature* creature = map->GetCreature(entity.Guid);
        if (!creature || !creature->IsAlive())
            continue;

        if (!anchor)
            anchor = creature;

        if (static_cast<TacticalRole>(entity.TacticalRole) == TacticalRole::Commander)
        {
            anchor = creature;
            break;
        }
    }

    if (!anchor)
        return;

    auto& triggered = _triggeredRouteActionIdsByGroup[group.Id];
    for (RouteNodeActionDefinition const& action : *actions)
    {
        if (triggered.find(action.Id) != triggered.end())
            continue;

        RouteNodeDefinition const* node = sLivingWorldDataMgr.GetRouteNode(action.RouteNodeId);
        if (!node || node->MapId != anchor->GetMapId())
            continue;

        if (anchor->GetDistance(node->X, node->Y, node->Z) > node->ArrivalRadius)
            continue;

        if (ExecuteRouteNodeAction(group, action))
            triggered.insert(action.Id);
    }
}

bool MovementController::ExecuteRouteNodeAction(RuntimeEntityGroup const& group, RouteNodeActionDefinition const& action)
{
    bool success = false;
    switch (action.ActionType)
    {
        case 1: // Dialogue: target_id=dialogue, parameter1=speaker spawn_member_id.
            success = sDialogueManager.Execute(
                group.RuntimeId,
                group.SpawnGroupId,
                action.TargetId,
                action.Parameter1);
            break;

        case 2: // Announcement: target_id=announcement, p1=scope, p2=scope id, p3=faction.
            success = sAnnouncementManager.Execute(
                group.RuntimeId,
                action.InvasionId,
                action.TargetId,
                action.Parameter1,
                action.Parameter2,
                action.Parameter3);
            break;

        case 3: // Sound: target_id=sound, parameter1=source member id, parameter2=playback mode.
            success = sSoundManager.Execute(
                group.RuntimeId,
                group.SpawnGroupId,
                action.TargetId,
                action.Parameter1,
                action.Parameter2);
            break;

        default:
            LOG_ERROR("server.loading",
                "[LW Route] Runtime entity group #{} encountered unsupported route-node action type {} for action {}.",
                group.Id, action.ActionType, action.Id);
            return false;
    }

    if (!success)
    {
        LOG_ERROR("server.loading",
            "[LW Route] Runtime entity group #{} failed route-node action {} type {} at route node {}.",
            group.Id, action.Id, action.ActionType, action.RouteNodeId);
        return false;
    }

    LOG_INFO("server.loading",
        "[LW Route] Runtime entity group #{} executed route-node action {} type {} at route node {}.",
        group.Id, action.Id, action.ActionType, action.RouteNodeId);
    return true;
}

bool MovementController::IsGroupMoving(uint64 runtimeGroupId) const
{
    return _activeMovements.find(runtimeGroupId) != _activeMovements.end();
}

void MovementController::Update(uint32 diff)
{
    if (_activeMovements.empty())
    {
        return;
    }

    if (_updateTimerMs > diff)
    {
        _updateTimerMs -= diff;
        return;
    }

    _updateTimerMs = MovementUpdateIntervalMs;
    uint64 const nowMs = static_cast<uint64>(getMSTime());
    std::vector<uint64> completed;

    for (auto& [runtimeGroupId, movement] : _activeMovements)
    {
        RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(runtimeGroupId);
        if (!group || group->State != RuntimeEntityGroupState::Active)
        {
            completed.push_back(runtimeGroupId);
            continue;
        }

        uint32 livingCreatures = 0;
        for (RuntimeEntity const& entity : group->Entities)
        {
            if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
            {
                continue;
            }

            Map* map = sMapMgr->FindMap(entity.MapId, 0);
            if (!map)
            {
                continue;
            }

            Creature* creature = map->GetCreature(entity.Guid);
            if (creature && creature->IsAlive())
            {
                ++livingCreatures;
            }
        }

        if (livingCreatures == 0)
        {
            LOG_INFO("server.loading",
                "[LW Movement] Runtime entity group #{} was defeated while following path {}. "
                "No living creature entities remain.",
                runtimeGroupId,
                movement.PathId);

            // A single runtime entity group being wiped out does not mean the
            // invasion itself has failed. MovementController only owns movement
            // lifecycle; invasion victory/defeat conditions are handled by the
            // configured GroupDefeatWatcher (including require-all semantics).
            completed.push_back(runtimeGroupId);
            continue;
        }

        if (movement.State == RuntimeMovementState::Completed)
        {
            completed.push_back(runtimeGroupId);
            continue;
        }

        if (movement.State == RuntimeMovementState::Waiting)
        {
            if (nowMs >= movement.WaitEndsAtMs)
            {
                AdvanceOrComplete(runtimeGroupId, movement, nowMs);
                if (movement.State == RuntimeMovementState::Completed)
                {
                    completed.push_back(runtimeGroupId);
                }
            }
            continue;
        }

        for (RuntimeMovementDestination& destination : movement.Destinations)
        {
            Map* map = sMapMgr->FindMap(destination.MapId, 0);
            if (!map)
            {
                continue;
            }

            Creature* creature = map->GetCreature(destination.Guid);
            if (creature && creature->IsAlive() && creature->IsInCombat())
            {
                destination.WasInCombat = true;
            }
        }

        ResumeInterruptedCreatures(movement);

        if (_activeRouteJourneys.find(runtimeGroupId) != _activeRouteJourneys.end())
            CheckRouteNodeActions(*group);

        if (!HasGroupReachedCurrentNode(movement, nowMs))
        {
            continue;
        }

        auto const* nodes = sLivingWorldDataMgr.GetMovementNodes(movement.PathId);
        if (!nodes || movement.NodeIndex >= nodes->size())
        {
            completed.push_back(runtimeGroupId);
            continue;
        }

        MovementNodeDefinition const& node = (*nodes)[movement.NodeIndex];

    uint64 const nowMs = getMSTime();

        uint32 actionWaitMs = 0;
        RuntimeEntityGroup* arrivedGroup = sRuntimeEntityGroupMgr.GetGroup(runtimeGroupId);
        if (arrivedGroup)
        {
            actionWaitMs = ExecuteNodeActions(movement, *arrivedGroup, node);
        }

        uint32 const effectiveWaitMs = std::max(node.WaitMs, actionWaitMs);
        if (effectiveWaitMs > 0)
        {
            movement.State = RuntimeMovementState::Waiting;
            movement.WaitEndsAtMs = nowMs + effectiveWaitMs;

            LOG_INFO("server.loading",
                "[LW Movement] Runtime entity group #{} reached node {} on path {} and will wait {} ms "
                "(node wait={}, action wait={}).",
                runtimeGroupId,
                node.NodeOrder,
                movement.PathId,
                effectiveWaitMs,
                node.WaitMs,
                actionWaitMs);
        }
        else
        {
            AdvanceOrComplete(runtimeGroupId, movement, nowMs);
            if (movement.State == RuntimeMovementState::Completed)
            {
                completed.push_back(runtimeGroupId);
            }
        }
    }

    for (uint64 runtimeGroupId : completed)
    {
        _activeMovements.erase(runtimeGroupId);
        _activeRouteJourneys.erase(runtimeGroupId);
        _triggeredRouteActionIdsByGroup.erase(runtimeGroupId);
    }

}

bool MovementController::BeginCurrentNode(ActiveRuntimeMovement& movement)
{
    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(movement.RuntimeGroupId);
    auto const* nodes = sLivingWorldDataMgr.GetMovementNodes(movement.PathId);

    if (!group || !nodes || movement.NodeIndex >= nodes->size())
    {
        return false;
    }

    MovementNodeDefinition const& node = (*nodes)[movement.NodeIndex];

    uint32 profileId = node.ProfileOverrideId != 0 ? node.ProfileOverrideId : movement.ProfileId;
    MovementProfileDefinition const* profile = profileId != 0
        ? sLivingWorldDataMgr.GetMovementProfile(profileId)
        : nullptr;

    // Route followers may be fighting or catching back up to the authored road
    // while the route leader advances through the dense 5-yard breadcrumbs.
    // Preserve only that small amount of state.  The old per-breadcrumb rejoin
    // cursor was intentionally removed because it could strand an entire force
    // repeatedly selecting a breadcrumb it was already standing on.
    struct PreviousRouteState
    {
        bool WasInCombat = false;
        bool RouteCatchupActive = false;
        bool RouteCatchupMoveIssued = false;
        uint64 RouteCatchupRefreshAtMs = 0;
        float RouteCatchupEndX = 0.0f;
        float RouteCatchupEndY = 0.0f;
        float RouteCatchupEndZ = 0.0f;
    };

    std::unordered_map<uint64, PreviousRouteState> previousRouteStates;
    if (movement.RouteMovement)
    {
        previousRouteStates.reserve(movement.Destinations.size());
        for (RuntimeMovementDestination const& previous : movement.Destinations)
        {
            previousRouteStates.emplace(
                previous.Guid.GetRawValue(),
                PreviousRouteState{
                    previous.WasInCombat,
                    previous.RouteCatchupActive,
                    previous.RouteCatchupMoveIssued,
                    previous.RouteCatchupRefreshAtMs,
                    previous.RouteCatchupEndX,
                    previous.RouteCatchupEndY,
                    previous.RouteCatchupEndZ });
        }
    }

    // A Commander is the preferred route-navigation owner.  Previously the
    // first spawned creature became slot 0, which meant a random rank-and-file
    // guard could stall a 96-NPC response force simply by entering combat.
    ObjectGuid preferredLeaderGuid;
    ObjectGuid fallbackLeaderGuid;
    if (movement.RouteMovement)
    {
        for (RuntimeEntity const& entity : group->Entities)
        {
            if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
                continue;

            Map* map = sMapMgr->FindMap(entity.MapId, 0);
            Creature* creature = map ? map->GetCreature(entity.Guid) : nullptr;
            if (!creature || !creature->IsAlive())
                continue;

            if (fallbackLeaderGuid.IsEmpty())
                fallbackLeaderGuid = entity.Guid;

            if (static_cast<TacticalRole>(entity.TacticalRole) == TacticalRole::Commander)
            {
                preferredLeaderGuid = entity.Guid;
                break;
            }
        }

        // Keep the route leader stable across node rebuilds.  Re-selecting a
        // commander at every 5-yard breadcrumb can make followers change
        // recovery targets mid-catch-up when a group contains more than one
        // Commander-role creature.  Only elect a new leader when the existing
        // leader is missing or dead.
        bool keepExistingLeader = false;
        if (!movement.RouteLeaderGuid.IsEmpty())
        {
            for (RuntimeEntity const& entity : group->Entities)
            {
                if (entity.Guid != movement.RouteLeaderGuid ||
                    entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
                    continue;

                Map* map = sMapMgr->FindMap(entity.MapId, 0);
                Creature* creature = map ? map->GetCreature(entity.Guid) : nullptr;
                keepExistingLeader = creature && creature->IsAlive();
                break;
            }
        }

        if (!keepExistingLeader)
            movement.RouteLeaderGuid = !preferredLeaderGuid.IsEmpty() ? preferredLeaderGuid : fallbackLeaderGuid;
    }

    uint32 moved = 0;
    std::unordered_map<uint8, uint32> roleSlots;
    uint32 followerMarchSlot = 1;
    movement.Destinations.clear();
    movement.ArrivalGraceStartedAtMs = 0;

    for (RuntimeEntity const& entity : group->Entities)
    {
        if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
        {
            continue;
        }

        Map* map = sMapMgr->FindMap(entity.MapId, 0);
        if (!map)
        {
            continue;
        }

        Creature* creature = map->GetCreature(entity.Guid);
        if (!creature || !creature->IsAlive())
        {
            continue;
        }

        if (profile)
        {
            // 0 = provider/default, 1 = walk, 2 = run.
            if (profile->DefaultMode == 1)
            {
                creature->SetWalk(true);
            }
            else if (profile->DefaultMode == 2)
            {
                creature->SetWalk(false);
            }
        }

        // Traveling caravans always walk, regardless of the movement profile
        // used by the underlying authored route.
        if (movement.RouteMovement &&
            group->RouteFormation == RouteFormationProfile::TravelingCaravan)
        {
            creature->SetWalk(true);
        }

        uint32 const roleSlot = roleSlots[entity.TacticalRole]++;
        uint32 const thisMarchSlot = movement.RouteMovement
            ? (entity.Guid == movement.RouteLeaderGuid ? 0U : followerMarchSlot++)
            : 0U;

        float targetX = node.X;
        float targetY = node.Y;
        float targetZ = node.Z;

        if (movement.RouteMovement)
            BuildMarchDestination(node, movement.Direction, thisMarchSlot, group->RouteFormation, targetX, targetY, targetZ);
        else
            BuildFormationDestination(node, movement.Direction, entity.TacticalRole, roleSlot, targetX, targetY, targetZ);

        RuntimeMovementDestination destination;
        destination.Guid = entity.Guid;
        destination.MapId = entity.MapId;
        destination.X = targetX;
        destination.Y = targetY;
        destination.Z = targetZ;

        if (movement.RouteMovement)
        {
            auto const stateItr = previousRouteStates.find(entity.Guid.GetRawValue());
            if (stateItr != previousRouteStates.end())
            {
                destination.WasInCombat = stateItr->second.WasInCombat;
                destination.RouteCatchupActive = stateItr->second.RouteCatchupActive;
                destination.RouteCatchupMoveIssued = stateItr->second.RouteCatchupMoveIssued;
                destination.RouteCatchupRefreshAtMs = stateItr->second.RouteCatchupRefreshAtMs;
                destination.RouteCatchupEndX = stateItr->second.RouteCatchupEndX;
                destination.RouteCatchupEndY = stateItr->second.RouteCatchupEndY;
                destination.RouteCatchupEndZ = stateItr->second.RouteCatchupEndZ;
            }

            // Never overwrite combat movement.  Likewise, a follower already
            // executing a breadcrumb-chain catch-up spline keeps that spline
            // while the commander advances through subsequent 5-yard nodes.
            if (creature->IsInCombat() || creature->GetVictim())
            {
                destination.WasInCombat = true;
                destination.RouteCatchupActive = false;
                destination.RouteCatchupMoveIssued = false;
                destination.RouteCatchupRefreshAtMs = 0;
                movement.Destinations.push_back(destination);
                ++moved;
                continue;
            }

            if (entity.Guid != movement.RouteLeaderGuid && destination.RouteCatchupActive)
            {
                movement.Destinations.push_back(destination);
                ++moved;
                continue;
            }
        }

        PathGenerator path(creature);

        // Do not force the destination. We want the navmesh to choose a valid,
        // terrain-aware endpoint rather than extending a failed path directly
        // through terrain to the requested XYZ.
        bool const pathFound = path.CalculatePath(
            targetX,
            targetY,
            targetZ,
            false);

        if (!pathFound || (path.GetPathType() & PATHFIND_NOPATH))
        {
            LOG_ERROR("server.loading",
                "[LW Movement] Runtime entity group #{} creature {} member {} could not find an MMAP path "
                "to path {} node {} formation destination ({:.2f}, {:.2f}, {:.2f}); creature skipped for this node.",
                movement.RuntimeGroupId,
                entity.Entry,
                entity.MemberId,
                movement.PathId,
                node.NodeOrder,
                targetX,
                targetY,
                targetZ);
            continue;
        }

        Movement::PointsArray pathPoints = path.GetPath();
        if (pathPoints.size() < 2)
        {
            LOG_ERROR("server.loading",
                "[LW Movement] Runtime entity group #{} creature {} member {} produced an unusable MMAP path "
                "with {} point(s) for path {} node {}; creature skipped for this node.",
                movement.RuntimeGroupId,
                entity.Entry,
                entity.MemberId,
                pathPoints.size(),
                movement.PathId,
                node.NodeOrder);
            continue;
        }

        G3D::Vector3 const& actualEnd = pathPoints.back();
        destination.X = actualEnd.x;
        destination.Y = actualEnd.y;
        destination.Z = actualEnd.z;
        movement.Destinations.push_back(destination);

        // MoveSplinePath consumes the terrain-aware point list generated by
        // AzerothCore's PathGenerator/MMAP system. This avoids PointMovement's
        // straight-line fallback when a requested destination cannot be reached.
        creature->GetMotionMaster()->MoveSplinePath(
            &pathPoints,
            FORCED_MOVEMENT_NONE);

        ++moved;
    }

    if (moved == 0)
    {
        LOG_ERROR("server.loading",
            "[LW Movement] Runtime entity group #{} has no living movable creature entities for path {} node {}.",
            movement.RuntimeGroupId, movement.PathId, node.NodeOrder);
        return false;
    }

    movement.State = RuntimeMovementState::Moving;
    movement.WaitEndsAtMs = 0;

    LOG_DEBUG("server.loading",
        "[LW Movement] Runtime entity group #{} moving {} creature(s) via MMAP in {} formation to path {} node {} ({:.2f}, {:.2f}, {:.2f}).",
        movement.RuntimeGroupId,
        moved,
        movement.RouteMovement ? "compact MARCH" : "role-aware",
        movement.PathId,
        node.NodeOrder,
        node.X,
        node.Y,
        node.Z);

    return true;
}

void MovementController::ResumeInterruptedCreatures(ActiveRuntimeMovement& movement)
{
    auto const* nodes = sLivingWorldDataMgr.GetMovementNodes(movement.PathId);
    if (!nodes || movement.NodeIndex >= nodes->size())
        return;

    MovementNodeDefinition const& node = (*nodes)[movement.NodeIndex];
    uint64 const nowMs = static_cast<uint64>(getMSTime());

    auto getCreature = [](RuntimeMovementDestination const& destination) -> Creature*
    {
        Map* map = sMapMgr->FindMap(destination.MapId, 0);
        return map ? map->GetCreature(destination.Guid) : nullptr;
    };

    // Keep one stable navigation owner for route travel.  The commander is
    // selected in BeginCurrentNode.  If that creature dies, choose the first
    // surviving creature until the next node rebuild can promote a commander
    // again if one exists.
    Creature* routeLeader = nullptr;
    if (movement.RouteMovement && !movement.RouteLeaderGuid.IsEmpty())
    {
        for (RuntimeMovementDestination const& destination : movement.Destinations)
        {
            if (destination.Guid != movement.RouteLeaderGuid)
                continue;

            Creature* creature = getCreature(destination);
            if (creature && creature->IsAlive())
                routeLeader = creature;
            break;
        }
    }

    if (movement.RouteMovement && !routeLeader)
    {
        for (RuntimeMovementDestination const& destination : movement.Destinations)
        {
            Creature* creature = getCreature(destination);
            if (!creature || !creature->IsAlive())
                continue;

            movement.RouteLeaderGuid = destination.Guid;
            routeLeader = creature;
            break;
        }
    }

    auto findReachableBreadcrumb = [&](Creature* creature, std::size_t& selectedIndex, float& selectedDistance) -> bool
    {
        std::vector<std::pair<float, std::size_t>> candidates;
        candidates.reserve(nodes->size());

        if (movement.Direction == MovementDirection::Forward)
        {
            for (std::size_t i = 0; i <= movement.NodeIndex && i < nodes->size(); ++i)
            {
                MovementNodeDefinition const& candidate = (*nodes)[i];
                candidates.emplace_back(creature->GetDistance(candidate.X, candidate.Y, candidate.Z), i);
            }
        }
        else
        {
            for (std::size_t i = movement.NodeIndex; i < nodes->size(); ++i)
            {
                MovementNodeDefinition const& candidate = (*nodes)[i];
                candidates.emplace_back(creature->GetDistance(candidate.X, candidate.Y, candidate.Z), i);
            }
        }

        if (candidates.empty())
            return false;

        std::sort(candidates.begin(), candidates.end(),
            [](auto const& left, auto const& right) { return left.first < right.first; });

        std::size_t const tryCount = std::min(candidates.size(), RouteRecoveryCandidateLimit);
        for (std::size_t n = 0; n < tryCount; ++n)
        {
            float const distance = candidates[n].first;
            std::size_t const index = candidates[n].second;

            // Standing on/near a breadcrumb is already a valid recovery point;
            // do not ask PathGenerator for a zero-length path and do not enter
            // a persistent "rejoin node" state.
            if (distance <= RouteBreadcrumbTolerance)
            {
                selectedIndex = index;
                selectedDistance = distance;
                return true;
            }

            MovementNodeDefinition const& candidate = (*nodes)[index];
            PathGenerator path(creature);
            bool const pathFound = path.CalculatePath(candidate.X, candidate.Y, candidate.Z, false);
            if (!pathFound || (path.GetPathType() & PATHFIND_NOPATH) || path.GetPath().size() < 2)
                continue;

            selectedIndex = index;
            selectedDistance = distance;
            return true;
        }

        return false;
    };

    auto launchRouteCatchup = [&](Creature* creature, RuntimeMovementDestination& destination) -> bool
    {
        std::size_t breadcrumbIndex = movement.NodeIndex;
        float breadcrumbDistance = 0.0f;
        if (!findReachableBreadcrumb(creature, breadcrumbIndex, breadcrumbDistance))
        {
            destination.RouteCatchupMoveIssued = false;
            destination.RouteCatchupRefreshAtMs = nowMs + RouteCatchupRefreshMs;
            LOG_WARN("server.loading",
                "[LW Movement] Runtime #{} group #{} entry={} guid={} could not find a reachable "
                "route breadcrumb on path {}; catch-up will retry in {} ms.",
                movement.RuntimeId,
                movement.RuntimeGroupId,
                creature->GetEntry(),
                creature->GetGUID().ToString(),
                movement.PathId,
                RouteCatchupRefreshMs);
            return false;
        }

        Movement::PointsArray catchupPoints;

        // If combat left the creature materially off the authored road, use
        // MMAP only for the short hop back to a reachable breadcrumb.  From
        // that point onward the dense recorded breadcrumbs are authoritative.
        if (breadcrumbDistance > RouteOffRoadRecoveryDistance)
        {
            MovementNodeDefinition const& recoveryNode = (*nodes)[breadcrumbIndex];
            PathGenerator path(creature);
            bool const pathFound = path.CalculatePath(recoveryNode.X, recoveryNode.Y, recoveryNode.Z, false);
            if (!pathFound || (path.GetPathType() & PATHFIND_NOPATH))
                return false;

            catchupPoints = path.GetPath();
            if (catchupPoints.size() < 2)
                return false;
        }
        else
        {
            catchupPoints.emplace_back(
                creature->GetPositionX(),
                creature->GetPositionY(),
                creature->GetPositionZ());
        }

        // Append every authored 5-yard breadcrumb between the creature and the
        // commander's current route target.  This is one continuous catch-up
        // spline, so there is no per-breadcrumb state machine to deadlock.
        if (movement.Direction == MovementDirection::Forward)
        {
            for (std::size_t i = breadcrumbIndex; i <= movement.NodeIndex && i < nodes->size(); ++i)
            {
                MovementNodeDefinition const& breadcrumb = (*nodes)[i];
                catchupPoints.emplace_back(breadcrumb.X, breadcrumb.Y, breadcrumb.Z);
            }
        }
        else
        {
            for (std::size_t i = breadcrumbIndex + 1; i-- > movement.NodeIndex; )
            {
                MovementNodeDefinition const& breadcrumb = (*nodes)[i];
                catchupPoints.emplace_back(breadcrumb.X, breadcrumb.Y, breadcrumb.Z);
                if (i == 0)
                    break;
            }
        }

        // Finish in this follower's compact MARCH slot, not at the centerline.
        catchupPoints.emplace_back(destination.X, destination.Y, destination.Z);

        if (catchupPoints.size() < 2)
            return false;

        creature->GetMotionMaster()->MoveSplinePath(&catchupPoints, FORCED_MOVEMENT_NONE);
        destination.RouteCatchupMoveIssued = true;
        destination.RouteCatchupEndX = destination.X;
        destination.RouteCatchupEndY = destination.Y;
        destination.RouteCatchupEndZ = destination.Z;
        // Single-flight: this timestamp is now a watchdog, not a periodic refresh.
        // Normal route-node advancement must not replace this spline while it is in flight.
        destination.RouteCatchupRefreshAtMs = nowMs + RouteCatchupWatchdogMs;

        LOG_INFO("server.loading",
            "[LW Movement] Runtime #{} group #{} entry={} guid={} launched route catch-up on path {} "
            "from breadcrumb {} to current node {} (off-road distance {:.2f} yd, {} spline points).",
            movement.RuntimeId,
            movement.RuntimeGroupId,
            creature->GetEntry(),
            creature->GetGUID().ToString(),
            movement.PathId,
            (*nodes)[breadcrumbIndex].NodeOrder,
            node.NodeOrder,
            breadcrumbDistance,
            catchupPoints.size());
        return true;
    };

    for (RuntimeMovementDestination& destination : movement.Destinations)
    {
        Creature* creature = getCreature(destination);
        if (!creature || !creature->IsAlive())
            continue;

        bool const inCombatNow = creature->IsInCombat() || creature->GetVictim();
        if (inCombatNow)
        {
            // This is the rising edge/latch for a combat episode.  WasInCombat
            // remains true until we observe the corresponding out-of-combat
            // edge below.  Do not manufacture post-combat transitions merely
            // because the creature remains out of combat on later updates.
            destination.WasInCombat = true;
            destination.RouteCatchupActive = false;
            destination.RouteCatchupMoveIssued = false;
            destination.RouteCatchupRefreshAtMs = 0;
            continue;
        }

        bool const isRouteLeader = movement.RouteMovement && destination.Guid == movement.RouteLeaderGuid;

        if (movement.RouteMovement && !isRouteLeader && routeLeader)
        {
            bool const justLeftCombat = destination.WasInCombat;
            if (justLeftCombat)
            {
                // Consume the falling edge exactly once.  RouteCatchupActive
                // is the persistent post-combat state; subsequent out-of-
                // combat updates must not re-enter recovery unless a real new
                // combat episode sets WasInCombat again.
                destination.WasInCombat = false;
                destination.RouteCatchupActive = true;
                destination.RouteCatchupMoveIssued = false;
                destination.RouteCatchupRefreshAtMs = 0;

                LOG_INFO("server.loading",
                    "[LW Movement] Runtime #{} group #{} entry={} guid={} left combat; "
                    "switching to breadcrumb-chain catch-up behind commander guid={}.",
                    movement.RuntimeId,
                    movement.RuntimeGroupId,
                    creature->GetEntry(),
                    creature->GetGUID().ToString(),
                    routeLeader->GetGUID().ToString());
            }

            if (!destination.RouteCatchupActive)
                continue;

            if (creature->GetDistance(routeLeader) <= RouteCatchupDistance)
            {
                destination.RouteCatchupActive = false;
                destination.RouteCatchupMoveIssued = false;
                destination.RouteCatchupRefreshAtMs = 0;

                // Snap back into the current compact MARCH destination once;
                // subsequent 5-yard node advances return to normal movement.
                PathGenerator path(creature);
                if (path.CalculatePath(destination.X, destination.Y, destination.Z, false) &&
                    !(path.GetPathType() & PATHFIND_NOPATH) && path.GetPath().size() >= 2)
                {
                    Movement::PointsArray pathPoints = path.GetPath();
                    creature->GetMotionMaster()->MoveSplinePath(&pathPoints, FORCED_MOVEMENT_NONE);
                }

                LOG_INFO("server.loading",
                    "[LW Movement] Runtime #{} group #{} entry={} guid={} completed route catch-up "
                    "within {:.2f} yd of commander; normal compact MARCH movement resumes.",
                    movement.RuntimeId,
                    movement.RuntimeGroupId,
                    creature->GetEntry(),
                    creature->GetGUID().ToString(),
                    creature->GetDistance(routeLeader));
                continue;
            }

            // SINGLE-FLIGHT catch-up.  Once a breadcrumb-chain spline is launched,
            // leave it completely alone while the commander continues advancing.
            // Only launch another leg after this creature reaches the endpoint that
            // was captured when the current leg started.
            if (destination.RouteCatchupMoveIssued)
            {
                float const endpointDistance = creature->GetDistance(
                    destination.RouteCatchupEndX,
                    destination.RouteCatchupEndY,
                    destination.RouteCatchupEndZ);

                if (endpointDistance <= RouteCatchupEndpointTolerance)
                {
                    destination.RouteCatchupMoveIssued = false;
                    destination.RouteCatchupRefreshAtMs = 0;

                    LOG_INFO("server.loading",
                        "[LW Movement] Runtime #{} group #{} entry={} guid={} finished route catch-up "
                        "leg at captured endpoint ({:.2f} yd); evaluating next leg against commander.",
                        movement.RuntimeId,
                        movement.RuntimeGroupId,
                        creature->GetEntry(),
                        creature->GetGUID().ToString(),
                        endpointDistance);
                }
                else if (nowMs < destination.RouteCatchupRefreshAtMs)
                {
                    // The current spline is still in flight.  Do not replace it merely
                    // because movement.NodeIndex/destination advanced another 5 yards.
                    continue;
                }
                else
                {
                    // Safety watchdog only: if AzerothCore stopped the spline without
                    // combat and without reaching its endpoint, allow one fresh leg.
                    destination.RouteCatchupMoveIssued = false;
                    destination.RouteCatchupRefreshAtMs = 0;
                    LOG_WARN("server.loading",
                        "[LW Movement] Runtime #{} group #{} entry={} guid={} route catch-up leg "
                        "watchdog expired {:.2f} yd from captured endpoint; rebuilding one leg.",
                        movement.RuntimeId,
                        movement.RuntimeGroupId,
                        creature->GetEntry(),
                        creature->GetGUID().ToString(),
                        endpointDistance);
                }
            }

            if (!destination.RouteCatchupMoveIssued && nowMs >= destination.RouteCatchupRefreshAtMs)
                launchRouteCatchup(creature, destination);

            continue;
        }

        // Route leader and all non-route movement retain exact-destination
        // resume.  Route progression pauses naturally while the commander is
        // fighting, so its authored target remains valid.
        if (creature->GetDistance(destination.X, destination.Y, destination.Z) <= ArrivalTolerance)
        {
            destination.WasInCombat = false;
            destination.RouteCatchupActive = false;
            destination.RouteCatchupMoveIssued = false;
            destination.RouteCatchupRefreshAtMs = 0;
            continue;
        }

        if (!destination.WasInCombat)
            continue;

        destination.WasInCombat = false;
        destination.RouteCatchupActive = false;
        destination.RouteCatchupMoveIssued = false;
        destination.RouteCatchupRefreshAtMs = 0;

        PathGenerator path(creature);
        bool const pathFound = path.CalculatePath(destination.X, destination.Y, destination.Z, false);
        if (!pathFound || (path.GetPathType() & PATHFIND_NOPATH))
        {
            LOG_ERROR("server.loading",
                "[LW Movement] Runtime #{} group #{} entry={} guid={} could not resume MMAP movement "
                "toward path {} node {} after combat.",
                movement.RuntimeId,
                movement.RuntimeGroupId,
                creature->GetEntry(),
                creature->GetGUID().ToString(),
                movement.PathId,
                node.NodeOrder);
            continue;
        }

        Movement::PointsArray pathPoints = path.GetPath();
        if (pathPoints.size() < 2)
            continue;

        G3D::Vector3 const& actualEnd = pathPoints.back();
        destination.X = actualEnd.x;
        destination.Y = actualEnd.y;
        destination.Z = actualEnd.z;
        creature->GetMotionMaster()->MoveSplinePath(&pathPoints, FORCED_MOVEMENT_NONE);
    }
}

bool MovementController::HasGroupReachedCurrentNode(
    ActiveRuntimeMovement& movement,
    uint64 nowMs)
{
    if (movement.Destinations.empty())
    {
        movement.ArrivalGraceStartedAtMs = 0;
        return false;
    }

    auto const* nodes = sLivingWorldDataMgr.GetMovementNodes(movement.PathId);
    if (!nodes || movement.NodeIndex >= nodes->size())
    {
        movement.ArrivalGraceStartedAtMs = 0;
        return false;
    }

    MovementNodeDefinition const& node = (*nodes)[movement.NodeIndex];

    // Route travel is intentionally leader-driven.  Followers are marching
    // formation passengers: combat, path correction, or a temporarily blocked
    // follower must never turn a dense 5-yard road path into stop-and-regroup
    // movement.  If the original leader dies, the first surviving destination
    // becomes the route leader so the journey can continue.
    if (movement.RouteMovement)
    {
        RuntimeMovementDestination const* leaderDestination = nullptr;
        for (RuntimeMovementDestination const& candidate : movement.Destinations)
        {
            if (candidate.Guid == movement.RouteLeaderGuid)
            {
                leaderDestination = &candidate;
                break;
            }
        }

        if (!leaderDestination)
        {
            movement.ArrivalGraceStartedAtMs = 0;
            return false;
        }

        Map* map = sMapMgr->FindMap(leaderDestination->MapId, 0);
        Creature* creature = map ? map->GetCreature(leaderDestination->Guid) : nullptr;
        if (!creature || !creature->IsAlive())
        {
            movement.ArrivalGraceStartedAtMs = 0;
            return false;
        }

        float const distance = creature->GetDistance(
            leaderDestination->X, leaderDestination->Y, leaderDestination->Z);
        if (distance <= ArrivalTolerance)
        {
            movement.ArrivalGraceStartedAtMs = 0;
            LOG_DEBUG("server.loading",
                "[LW Movement] Runtime #{} group #{} commander/route leader guid={} reached path {} "
                "node {} (distance {:.2f}); advancing without waiting for followers.",
                movement.RuntimeId,
                movement.RuntimeGroupId,
                creature->GetGUID().ToString(),
                movement.PathId,
                node.NodeOrder,
                distance);
            return true;
        }

        return false;
    }

    bool const isFinalNode = movement.Direction == MovementDirection::Reverse
        ? movement.NodeIndex == 0
        : movement.NodeIndex + 1 >= nodes->size();

    uint32 living = 0;
    uint32 exactArrivals = 0;
    uint32 formationArrivals = 0;
    uint32 objectiveArrivals = 0;
    bool anyInCombat = false;

    for (RuntimeMovementDestination const& destination : movement.Destinations)
    {
        Map* map = sMapMgr->FindMap(destination.MapId, 0);
        if (!map)
            continue;

        Creature* creature = map->GetCreature(destination.Guid);
        if (!creature || !creature->IsAlive())
            continue;

        ++living;
        if (creature->IsInCombat())
            anyInCombat = true;

        float const formationDistance = creature->GetDistance(
            destination.X, destination.Y, destination.Z);

        if (formationDistance <= ArrivalTolerance)
        {
            ++exactArrivals;
            ++formationArrivals;
        }
        else if (formationDistance <= FormationArrivalTolerance)
        {
            ++formationArrivals;
        }

        if (creature->GetDistance(node.X, node.Y, node.Z) <= FinalObjectiveArrivalRadius)
            ++objectiveArrivals;
    }

    if (living == 0)
    {
        movement.ArrivalGraceStartedAtMs = 0;
        return false;
    }

    uint32 const requiredArrivals =
        std::max<uint32>(1, (living * FormationArrivalPercent + 99) / 100);

    if (isFinalNode && objectiveArrivals >= requiredArrivals)
    {
        LOG_INFO("server.loading",
            "[LW Movement] Runtime entity group #{} reached final objective for path {} node {}: "
            "{}/{} surviving creature(s) within {:.1f} yards. Combat does not block final arrival.",
            movement.RuntimeGroupId, movement.PathId, node.NodeOrder,
            objectiveArrivals, living, FinalObjectiveArrivalRadius);
        movement.ArrivalGraceStartedAtMs = 0;
        return true;
    }

    if (!isFinalNode && exactArrivals == living)
    {
        movement.ArrivalGraceStartedAtMs = 0;
        return true;
    }

    if (!isFinalNode && anyInCombat)
    {
        movement.ArrivalGraceStartedAtMs = 0;
        return false;
    }

    if (isFinalNode)
    {
        movement.ArrivalGraceStartedAtMs = 0;
        return false;
    }

    if (formationArrivals < requiredArrivals)
    {
        movement.ArrivalGraceStartedAtMs = 0;
        return false;
    }

    if (movement.ArrivalGraceStartedAtMs == 0)
    {
        movement.ArrivalGraceStartedAtMs = nowMs;
        LOG_DEBUG("server.loading",
            "[LW Movement] Runtime entity group #{} has {}/{} surviving creature(s) within {:.1f} yards "
            "of their path {} node formation destinations; starting {} ms regroup grace.",
            movement.RuntimeGroupId, formationArrivals, living, FormationArrivalTolerance,
            movement.PathId, FormationArrivalGraceMs);
        return false;
    }

    if (nowMs - movement.ArrivalGraceStartedAtMs < FormationArrivalGraceMs)
        return false;

    LOG_DEBUG("server.loading",
        "[LW Movement] Runtime entity group #{} accepted formation arrival at path {} node with "
        "{}/{} surviving creature(s) in position after regroup grace.",
        movement.RuntimeGroupId, movement.PathId, formationArrivals, living);

    movement.ArrivalGraceStartedAtMs = 0;
    return true;
}


void MovementController::AdvanceOrComplete(
    uint64 runtimeGroupId,
    ActiveRuntimeMovement& movement,
    uint64 /*nowMs*/)
{
    auto const* nodes = sLivingWorldDataMgr.GetMovementNodes(movement.PathId);
    if (!nodes)
    {
        movement.State = RuntimeMovementState::Completed;
        return;
    }

    if (movement.Direction == MovementDirection::Reverse)
    {
        if (movement.NodeIndex == 0)
        {
            CompleteMovement(runtimeGroupId, movement);
            return;
        }

        --movement.NodeIndex;
    }
    else
    {
        ++movement.NodeIndex;

        if (movement.NodeIndex >= nodes->size())
        {
            CompleteMovement(runtimeGroupId, movement);
            return;
        }
    }

    if (!BeginCurrentNode(movement))
    {
        LOG_ERROR("server.loading",
            "[LW Movement] Runtime entity group #{} failed to begin the next node on path {}.",
            runtimeGroupId, movement.PathId);
        movement.State = RuntimeMovementState::Completed;
    }
}

void MovementController::CompleteMovement(
    uint64 runtimeGroupId,
    ActiveRuntimeMovement& movement)
{
    // The final role-aware/MMAP destination becomes each surviving creature's
    // new home position. If combat later pulls an invader away and its AI
    // evades/resets, AzerothCore will return it to the objective it actually
    // reached instead of the group's original spawn/staging position.
    uint32 homesUpdated = 0;

    for (RuntimeMovementDestination const& destination : movement.Destinations)
    {
        Map* map = sMapMgr->FindMap(destination.MapId, 0);
        if (!map)
        {
            continue;
        }

        Creature* creature = map->GetCreature(destination.Guid);
        if (!creature || !creature->IsAlive())
        {
            continue;
        }

        creature->SetHomePosition(
            destination.X,
            destination.Y,
            destination.Z,
            creature->GetOrientation());

        ++homesUpdated;
    }

    LOG_INFO("server.loading",
        "[LW Movement] Runtime entity group #{} completed movement path {}; "
        "updated {} surviving creature home position(s) to their final formation destinations.",
        runtimeGroupId, movement.PathId, homesUpdated);

    auto journeyItr = _activeRouteJourneys.find(runtimeGroupId);
    if (journeyItr != _activeRouteJourneys.end())
    {
        ActiveRouteJourney& journey = journeyItr->second;
        RouteJourneyStep const& completedStep = journey.Steps[journey.StepIndex];
        NotifyRouteNodeReached(runtimeGroupId, completedStep.ToNodeId);

        if (journey.StepIndex + 1 < journey.Steps.size())
        {
            ++journey.StepIndex;
            RouteJourneyStep const nextStep = journey.Steps[journey.StepIndex];
            uint32 const nextCompletionSignalId = journey.StepIndex + 1 == journey.Steps.size()
                ? journey.CompletionSignalId
                : 0;

            if (StartRouteSegment(
                    runtimeGroupId,
                    nextStep.SegmentId,
                    nextStep.FromNodeId,
                    journey.ProfileId,
                    nextCompletionSignalId))
            {
                LOG_INFO("server.loading",
                    "[LW Route] Runtime entity group #{} advanced to route journey segment {}/{} (segment {}, node {} -> {}).",
                    runtimeGroupId,
                    journey.StepIndex + 1,
                    journey.Steps.size(),
                    nextStep.SegmentId,
                    nextStep.FromNodeId,
                    nextStep.ToNodeId);
                return;
            }

            LOG_ERROR("server.loading",
                "[LW Route] Runtime entity group #{} failed to continue route journey on segment {} from node {}; journey aborted.",
                runtimeGroupId, nextStep.SegmentId, nextStep.FromNodeId);
            _activeRouteJourneys.erase(journeyItr);
            movement.State = RuntimeMovementState::Completed;
            return;
        }

        LOG_INFO("server.loading",
            "[LW Route] Runtime entity group #{} completed route journey from node {} to node {} across {} segment(s).",
            runtimeGroupId,
            journey.StartNodeId,
            journey.DestinationNodeId,
            journey.Steps.size());
        _activeRouteJourneys.erase(journeyItr);
    }

    if (movement.CompletionSignalId != 0)
    {
        if (!sRuntimeSignalMgr.EmitSignal(movement.RuntimeId, movement.CompletionSignalId))
        {
            LOG_ERROR("server.loading",
                "[LW Movement] Runtime entity group #{} failed to emit completion signal {} for runtime #{}.",
                runtimeGroupId, movement.CompletionSignalId, movement.RuntimeId);
        }
    }

    movement.State = RuntimeMovementState::Completed;
}
}
