#ifndef MOD_LIVING_WORLD_TRAVELING_EVENT_MANAGER_H
#define MOD_LIVING_WORLD_TRAVELING_EVENT_MANAGER_H

#include "Define.h"
#include "ObjectGuid.h"

#include <string>
#include <unordered_map>
#include <vector>

class Creature;

namespace lw
{
enum class TravelingEventState : uint8 { Traveling = 0, Camped = 1 };
enum class TravelingEventStartResult : uint8 { Started = 0, NotFound, Disabled, AlreadyActive, InvalidConfiguration, SpawnFailed };
enum class TravelingTraversalMode : uint8 { Loop = 0, PingPong = 1, OneWay = 2 };

struct TravelingMemberDefinition
{
    uint32 Id = 0;
    uint32 EventId = 0;
    uint16 MemberOrder = 0;
    std::string MemberKey;
    uint32 CreatureEntry = 0;
    bool IsLeader = false;
    bool VendorWhileCamped = false;
    bool Enabled = true;
};

struct TravelingStopDefinition
{
    uint32 Id = 0;
    uint32 EventId = 0;
    uint32 StopOrder = 0;
    uint32 RouteNodeId = 0;
    uint32 CampLayoutId = 0;
    uint32 DwellSeconds = 120;
};

struct TravelingLegDefinition
{
    uint32 Id = 0;
    uint32 EventId = 0;
    uint32 FromStopId = 0;
    uint32 ToStopId = 0;
    uint32 SpeakerMemberId = 0;
    std::string DepartureText;
    std::string ArrivalText;
    bool Enabled = true;
};

struct TravelingCampMemberPlacementDefinition
{
    uint32 Id = 0;
    uint32 LayoutId = 0;
    std::string MemberKey;
    float ForwardOffset = 0.0f;
    float RightOffset = 0.0f;
    float ZOffset = 0.0f;
    float OrientationOffset = 0.0f;
};

struct TravelingCampPropDefinition
{
    uint32 Id = 0;
    uint32 LayoutId = 0;
    uint32 GameObjectEntry = 0;
    float ForwardOffset = 0.0f;
    float RightOffset = 0.0f;
    float ZOffset = 0.0f;
    float OrientationOffset = 0.0f;
};

enum class TravelingCampTargetType : uint8 { MemberPlacement = 1, LayoutProp = 2 };

struct TravelingCampNodeZOverride
{
    uint32 RouteNodeId = 0;
    TravelingCampTargetType TargetType = TravelingCampTargetType::LayoutProp;
    uint32 TargetId = 0;
    float ZOverride = 0.0f;
};

struct TravelingCampLayoutDefinition
{
    uint32 Id = 0;
    std::string Name;
    bool Enabled = true;
    std::vector<TravelingCampMemberPlacementDefinition> Members;
    std::vector<TravelingCampPropDefinition> Props;
};

struct TravelingEventDefinition
{
    uint32 Id = 0;
    std::string Name;
    TravelingTraversalMode TraversalMode = TravelingTraversalMode::Loop;
    bool AutoStart = false;
    bool Enabled = false;
    std::vector<TravelingMemberDefinition> Members;
    std::vector<TravelingStopDefinition> Stops;
    std::vector<TravelingLegDefinition> Legs;
};

struct ActiveTravelingEvent
{
    uint32 EventId = 0;
    uint64 RuntimeGroupId = 0;
    uint32 StopIndex = 0;
    int8 TravelDirection = 1;
    uint32 ActiveLegId = 0;
    bool JourneyComplete = false;
    TravelingEventState State = TravelingEventState::Camped;
    uint32 StateTimerMs = 0;
    uint16 MapId = 0;
    ObjectGuid LeaderGuid;
    std::unordered_map<uint32, ObjectGuid> MemberGuids;
    std::vector<ObjectGuid> CampPropGuids;
};

class TravelingEventManager
{
public:
    static TravelingEventManager& Instance();
    void LoadDefinitions();
    void Reset();
    void Update(uint32 diff);
    void ConfigureTravelWindow(uint32 startHour, uint32 endHour);
    void StartAutoEvents();
    TravelingEventStartResult Start(uint32 eventId, std::string* error = nullptr);
    bool Stop(uint32 eventId, std::string* error = nullptr);
    bool DebugSpawnCamp(uint32 eventId, uint32 routeNodeId, std::string* error = nullptr);
    bool DebugDespawnCamp(uint32 eventId, uint32 routeNodeId, std::string* error = nullptr);
    std::string BuildStatusReport() const;

private:
    TravelingEventDefinition const* GetDefinition(uint32 eventId) const;
    TravelingCampLayoutDefinition const* GetCampLayout(uint32 layoutId) const;
    TravelingMemberDefinition const* GetLeaderDefinition(TravelingEventDefinition const& definition) const;
    TravelingMemberDefinition const* GetMemberDefinition(TravelingEventDefinition const& definition, uint32 memberId) const;
    TravelingMemberDefinition const* GetMemberDefinitionByKey(TravelingEventDefinition const& definition, std::string const& memberKey) const;
    TravelingLegDefinition const* GetLeg(TravelingEventDefinition const& definition, uint32 fromStopId, uint32 toStopId) const;
    TravelingLegDefinition const* GetLegById(TravelingEventDefinition const& definition, uint32 legId) const;
    float GetCampNodeZOverride(uint32 routeNodeId, TravelingCampTargetType targetType, uint32 targetId) const;
    bool SpawnMembersAtStop(TravelingEventDefinition const& definition, uint32 stopIndex, ActiveTravelingEvent& runtime, bool createMovementGroup, std::string* error);
    bool BeginTravel(ActiveTravelingEvent& runtime, TravelingEventDefinition const& definition);
    bool BeginCamp(ActiveTravelingEvent& runtime, TravelingEventDefinition const& definition);
    void EndCamp(ActiveTravelingEvent& runtime, TravelingEventDefinition const& definition);
    void CleanupRuntime(ActiveTravelingEvent& runtime);
    bool ResolveNextStopIndex(ActiveTravelingEvent& runtime, TravelingEventDefinition const& definition, uint32& toIndex);
    uint64 MakeDebugCampKey(uint32 eventId, uint32 routeNodeId) const;
    uint8 GetLocalHour() const;
    bool IsTravelWindowOpen() const;
    ObjectGuid GetMemberGuid(ActiveTravelingEvent const& runtime, uint32 memberId) const;
    Creature* GetMemberCreature(ActiveTravelingEvent const& runtime, TravelingMemberDefinition const& member) const;
    Creature* GetCreature(uint16 mapId, ObjectGuid guid) const;
    Creature* GetLegSpeaker(ActiveTravelingEvent const& runtime, TravelingEventDefinition const& definition, TravelingLegDefinition const* leg) const;
    void ApplyProtectedState(Creature* creature) const;
    void ApplyTravelServices(ActiveTravelingEvent& runtime, TravelingEventDefinition const& definition) const;
    void ApplyCampServices(ActiveTravelingEvent& runtime, TravelingEventDefinition const& definition) const;

    std::unordered_map<uint32, TravelingCampLayoutDefinition> _campLayouts;
    std::unordered_map<uint64, TravelingCampNodeZOverride> _campNodeZOverrides;
    std::unordered_map<uint32, TravelingEventDefinition> _definitions;
    std::unordered_map<uint32, ActiveTravelingEvent> _active;
    std::unordered_map<uint64, ActiveTravelingEvent> _debugCamps;
    uint8 _travelStartHour = 6;
    uint8 _travelEndHour = 18;
};
}

#define sTravelingEventMgr lw::TravelingEventManager::Instance()
#endif
