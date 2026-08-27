#ifndef MOD_LIVING_WORLD_H
#define MOD_LIVING_WORLD_H

#include "Define.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace lw
{
struct ResponseOriginDefinition
{
    uint32 Id = 0;
    std::string Name;
    uint16 MapId = 0;
    uint8 Team = 0;
    uint32 MaxActiveDefault = 1;
    bool Enabled = false;
};


struct StageActionDefinition
{
    uint32 Id = 0;
    uint32 StageId = 0;
    uint16 ActionOrder = 0;
    uint8 ActionType = 1;
    uint32 TargetId = 0;
    uint32 Parameter1 = 0;
    uint32 Parameter2 = 0;
    uint32 Parameter3 = 0;
    uint32 Parameter4 = 0;
    uint32 DelaySeconds = 0;
    bool Enabled = false;
};

enum class TacticalRole : uint8
{
    Default = 0,
    Commander = 1,
    Protector = 2,
    MeleeDps = 3,
    RangedDps = 4,
    Healer = 5,
    Support = 6
};

struct SpawnMemberDefinition
{
    uint32 Id = 0;
    uint32 SpawnGroupId = 0;
    uint8 EntityType = 1;
    uint32 EntityEntry = 0;
    uint32 LwTemplateId = 0;
    uint16 Count = 1;
    uint16 LevelOverride = 0;
    TacticalRole Role = TacticalRole::Default;
    std::string Comment;
};

struct SpawnGroupDefinition
{
    uint32 Id = 0;
    std::string Name;
    uint32 RouteNodeId = 0;
    float SpawnRadius = 5;
    bool Enabled = false;

    // Runtime-only explicit anchor used by systems such as garrison restock.
    // Authored invasion spawn groups use RouteNodeId instead of raw XYZ.
    bool UseExplicitPosition = false;
    uint16 MapId = 0;
    float X = 0;
    float Y = 0;
    float Z = 0;
    float Orientation = 0;
};



struct DialogueDefinition
{
    uint32 Id = 0;
    std::string Name;
    std::string Text;
    uint8 ChatType = 0;
    uint8 Language = 0;
    bool Enabled = false;
    std::string Comment;
};

struct AnnouncementDefinition
{
    uint32 Id = 0;
    std::string Name;
    std::string Text;
    bool Enabled = false;
    std::string Comment;
};

struct RuntimeSignalDefinition
{
    uint32 Id = 0;
    std::string Name;
    bool Enabled = false;
    std::string Comment;
};

struct MovementNodeActionDefinition
{
    uint32 Id = 0;
    uint32 NodeId = 0;
    uint16 ActionOrder = 0;
    uint8 ActionType = 1;
    uint32 TargetId = 0;
    uint32 Parameter1 = 0;
    uint32 Parameter2 = 0;
    uint32 Parameter3 = 0;
    bool Enabled = false;
    std::string Comment;
};

struct MovementPathDefinition
{
    uint32 Id = 0;
    std::string Name;
    bool Enabled = false;
    std::string Comment;
};

struct MovementNodeDefinition
{
    uint32 Id = 0;
    uint32 PathId = 0;
    uint16 NodeOrder = 0;
    uint16 MapId = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Orientation = 0.0f;
    uint32 WaitMs = 0;
    uint32 ProfileOverrideId = 0;
    bool Enabled = false;
    std::string Comment;
};

struct MovementProfileDefinition
{
    uint32 Id = 0;
    std::string Name;
    uint8 DefaultMode = 0;
    float WalkSpeedMultiplier = 1.0f;
    float RunSpeedMultiplier = 1.0f;
    bool StealthEnabled = false;
    bool Enabled = false;
    std::string Comment;
};

struct RouteNodeDefinition
{
    uint32 Id = 0;
    std::string Name;
    uint16 MapId = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Orientation = 0.0f;
    float ArrivalRadius = 5.0f;
    bool Enabled = false;
    std::string Comment;
};


struct RouteNodeActionDefinition
{
    uint32 Id = 0;
    uint32 InvasionId = 0;
    uint32 SpawnGroupId = 0;
    uint32 RouteNodeId = 0;
    uint16 ActionOrder = 0;
    uint8 ActionType = 1;
    uint32 TargetId = 0;
    uint32 Parameter1 = 0;
    uint32 Parameter2 = 0;
    uint32 Parameter3 = 0;
    bool Enabled = false;
    std::string Comment;
};

struct RouteSegmentDefinition
{
    uint32 Id = 0;
    std::string Name;
    uint32 StartNodeId = 0;
    uint32 EndNodeId = 0;
    uint32 MovementPathId = 0;
    bool Enabled = false;
    std::string Comment;
};

struct InvasionStageDefinition
{
    uint32 Id = 0;
    uint32 InvasionId = 0;
    uint16 StageOrder = 0;
    std::string Name;
    uint32 DurationSeconds = 30;
    uint8 CompletionType = 0;
    uint32 CompletionTargetId = 0;
    bool Enabled = false;
};

struct InvasionDefinition
{
    uint32 Id = 0;
    std::string Name;
    uint16 MapId = 0;
    uint32 ZoneId = 0;
    uint8 Team = 0;
    uint32 ResponseOriginId = 0;
    uint8 RecommendedMinLevel = 1;
    uint8 RecommendedMaxLevel = 80;
    uint32 SelectionWeight = 100;
    uint32 MinimumCooldownSeconds = 86400;
    uint32 MaximumCooldownSeconds = 604800;
    uint32 MaximumRuntimeSeconds = 1800;
    bool AllowRandomStart = true;
    bool Enabled = false;
};

class LivingWorldDataMgr
{
public:
    static LivingWorldDataMgr& Instance();

    void LoadDefinitions();
    void Clear();

    [[nodiscard]] ResponseOriginDefinition const* GetResponseOrigin(uint32 responseOriginId) const;
    [[nodiscard]] InvasionDefinition const* GetDefinition(uint32 invasionId) const;
    [[nodiscard]] std::unordered_map<uint32, ResponseOriginDefinition> const& GetResponseOrigins() const;
    [[nodiscard]] std::unordered_map<uint32, InvasionDefinition> const& GetDefinitions() const;
    [[nodiscard]] std::vector<InvasionStageDefinition> const* GetStages(uint32 invasionId) const;
    [[nodiscard]] std::vector<StageActionDefinition> const* GetActions(uint32 stageId) const;
    [[nodiscard]] SpawnGroupDefinition const* GetSpawnGroup(uint32 id) const;
    [[nodiscard]] std::vector<SpawnMemberDefinition> const* GetSpawnMembers(uint32 id) const;
    [[nodiscard]] MovementPathDefinition const* GetMovementPath(uint32 id) const;
    [[nodiscard]] std::vector<MovementNodeDefinition> const* GetMovementNodes(uint32 pathId) const;
    [[nodiscard]] std::vector<MovementNodeActionDefinition> const* GetMovementNodeActions(uint32 nodeId) const;
    [[nodiscard]] MovementProfileDefinition const* GetMovementProfile(uint32 id) const;
    [[nodiscard]] RouteNodeDefinition const* GetRouteNode(uint32 id) const;
    [[nodiscard]] RouteNodeDefinition const* GetRouteNode(std::string const& name) const;
    [[nodiscard]] RouteSegmentDefinition const* GetRouteSegment(uint32 id) const;
    [[nodiscard]] RouteSegmentDefinition const* GetRouteSegment(std::string const& name) const;
    [[nodiscard]] std::unordered_map<uint32, RouteSegmentDefinition> const& GetRouteSegments() const;
    [[nodiscard]] std::vector<RouteNodeActionDefinition> const* GetRouteNodeActions(uint32 invasionId, uint32 spawnGroupId) const;
    [[nodiscard]] RuntimeSignalDefinition const* GetRuntimeSignal(uint32 id) const;
    [[nodiscard]] DialogueDefinition const* GetDialogue(uint32 id) const;
    [[nodiscard]] AnnouncementDefinition const* GetAnnouncement(uint32 id) const;

    [[nodiscard]] std::size_t GetDefinitionCount() const;
    [[nodiscard]] std::size_t GetResponseOriginCount() const;
    [[nodiscard]] std::size_t GetStageCount() const;
    [[nodiscard]] std::size_t GetActionCount() const;
    [[nodiscard]] std::size_t GetSpawnGroupCount() const;
    [[nodiscard]] std::size_t GetSpawnMemberCount() const;
    [[nodiscard]] std::size_t GetMovementPathCount() const;
    [[nodiscard]] std::size_t GetMovementNodeCount() const;
    [[nodiscard]] std::size_t GetMovementNodeActionCount() const;
    [[nodiscard]] std::size_t GetMovementProfileCount() const;
    [[nodiscard]] std::size_t GetRouteNodeCount() const;
    [[nodiscard]] std::size_t GetRouteSegmentCount() const;
    [[nodiscard]] std::size_t GetRouteNodeActionCount() const;
    [[nodiscard]] std::size_t GetRuntimeSignalCount() const;
    [[nodiscard]] std::size_t GetDialogueCount() const;
    [[nodiscard]] std::size_t GetAnnouncementCount() const;

private:
    LivingWorldDataMgr() = default;

    std::unordered_map<uint32, ResponseOriginDefinition> _responseOrigins;
    std::unordered_map<uint32, InvasionDefinition> _definitions;
    std::unordered_map<uint32, std::vector<InvasionStageDefinition>> _stagesByInvasion;
    std::unordered_map<uint32, std::vector<StageActionDefinition>> _actionsByStage;
    std::unordered_map<uint32, SpawnGroupDefinition> _spawnGroups;
    std::unordered_map<uint32, std::vector<SpawnMemberDefinition>> _spawnMembersByGroup;
    std::unordered_map<uint32, MovementPathDefinition> _movementPaths;
    std::unordered_map<uint32, std::vector<MovementNodeDefinition>> _movementNodesByPath;
    std::unordered_map<uint32, std::vector<MovementNodeActionDefinition>> _movementNodeActionsByNode;
    std::unordered_map<uint32, MovementProfileDefinition> _movementProfiles;
    std::unordered_map<uint32, RouteNodeDefinition> _routeNodes;
    std::unordered_map<uint32, RouteSegmentDefinition> _routeSegments;
    std::unordered_map<uint64, std::vector<RouteNodeActionDefinition>> _routeNodeActionsByInvasionGroup;
    std::unordered_map<uint32, RuntimeSignalDefinition> _runtimeSignals;
    std::unordered_map<uint32, DialogueDefinition> _dialogues;
    std::unordered_map<uint32, AnnouncementDefinition> _announcements;
};
}

#define sLivingWorldDataMgr lw::LivingWorldDataMgr::Instance()

#endif
