#ifndef MOD_LIVING_WORLD_MOVEMENT_CONTROLLER_H
#define MOD_LIVING_WORLD_MOVEMENT_CONTROLLER_H

#include "Define.h"
#include "ObjectGuid.h"

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lw
{
struct RuntimeEntityGroup;
struct RouteNodeActionDefinition;

enum class RuntimeMovementState : uint8
{
    Moving = 0,
    Waiting = 1,
    Completed = 2
};

enum class MovementDirection : uint8
{
    Forward = 0,
    Reverse = 1
};

struct RuntimeMovementDestination
{
    ObjectGuid Guid;
    uint32 MapId = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    bool WasInCombat = false;
    bool RouteCatchupActive = false;
    bool RouteCatchupMoveIssued = false;
    uint64 RouteCatchupRefreshAtMs = 0;
    float RouteCatchupEndX = 0.0f;
    float RouteCatchupEndY = 0.0f;
    float RouteCatchupEndZ = 0.0f;
};


struct RouteJourneyStep
{
    uint32 SegmentId = 0;
    uint32 FromNodeId = 0;
    uint32 ToNodeId = 0;
};

struct ActiveRouteJourney
{
    uint64 RuntimeGroupId = 0;
    uint64 RuntimeId = 0;
    uint32 StartNodeId = 0;
    uint32 DestinationNodeId = 0;
    uint32 ProfileId = 0;
    uint32 CompletionSignalId = 0;
    std::size_t StepIndex = 0;
    std::vector<RouteJourneyStep> Steps;
};

struct ActiveRuntimeMovement
{
    uint64 RuntimeGroupId = 0;
    uint64 RuntimeId = 0;
    uint32 PathId = 0;
    uint32 ProfileId = 0;
    uint32 CompletionSignalId = 0;
    MovementDirection Direction = MovementDirection::Forward;
    std::size_t NodeIndex = 0;
    RuntimeMovementState State = RuntimeMovementState::Moving;
    uint64 WaitEndsAtMs = 0;
    uint64 ArrivalGraceStartedAtMs = 0;
    bool RouteMovement = false;
    ObjectGuid RouteLeaderGuid;
    std::vector<RuntimeMovementDestination> Destinations;
};

class MovementController
{
public:
    static MovementController& Instance();

    void Reset();
    void Update(uint32 diff);

    bool StartPath(
        uint64 runtimeGroupId,
        uint32 pathId,
        uint32 profileId = 0,
        uint32 completionSignalId = 0,
        MovementDirection direction = MovementDirection::Forward,
        bool routeMovement = false);

    bool StartRouteSegment(
        uint64 runtimeGroupId,
        uint32 routeSegmentId,
        uint32 fromNodeId,
        uint32 profileId = 0,
        uint32 completionSignalId = 0);

    bool StartRouteJourney(
        uint64 runtimeGroupId,
        uint32 fromNodeId,
        uint32 destinationNodeId,
        uint32 profileId = 0,
        uint32 completionSignalId = 0);

    bool CancelGroup(uint64 runtimeGroupId);
    void NotifyRouteNodeReached(uint64 runtimeGroupId, uint32 routeNodeId, uint32 invasionId = 0);
    void CancelRuntime(uint64 runtimeId);

    [[nodiscard]] bool IsGroupMoving(uint64 runtimeGroupId) const;

private:
    MovementController() = default;

    bool BeginCurrentNode(ActiveRuntimeMovement& movement);
    void ResumeInterruptedCreatures(ActiveRuntimeMovement& movement);
    bool HasGroupReachedCurrentNode(ActiveRuntimeMovement& movement, uint64 nowMs);
    void AdvanceOrComplete(uint64 runtimeGroupId, ActiveRuntimeMovement& movement, uint64 nowMs);
    void CompleteMovement(uint64 runtimeGroupId, ActiveRuntimeMovement& movement);
    bool BuildRouteJourney(uint32 fromNodeId, uint32 destinationNodeId, std::vector<RouteJourneyStep>& steps) const;
    void CheckRouteNodeActions(RuntimeEntityGroup const& group);
    bool ExecuteRouteNodeAction(RuntimeEntityGroup const& group, RouteNodeActionDefinition const& action);

    std::unordered_map<uint64, ActiveRuntimeMovement> _activeMovements;
    std::unordered_map<uint64, ActiveRouteJourney> _activeRouteJourneys;
    std::unordered_map<uint64, std::unordered_set<uint32>> _triggeredRouteActionIdsByGroup;
    uint32 _updateTimerMs = 0;
};
}

#define sMovementController lw::MovementController::Instance()

#endif
