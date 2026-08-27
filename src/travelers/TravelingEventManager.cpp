#include "TravelingEventManager.h"

#include "Creature.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "GameObject.h"
#include "LivingWorld.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "MovementController.h"
#include "QueryResult.h"
#include "RuntimeEntityGroup.h"
#include "TemporarySummon.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>

namespace lw
{
TravelingEventManager& TravelingEventManager::Instance()
{
    static TravelingEventManager instance;
    return instance;
}

void TravelingEventManager::ConfigureTravelWindow(uint32 startHour, uint32 endHour)
{
    if (startHour > 23) { LOG_WARN("server.loading", "[LW Travel] StartHour {} is invalid; using 6.", startHour); startHour = 6; }
    if (endHour > 23) { LOG_WARN("server.loading", "[LW Travel] EndHour {} is invalid; using 18.", endHour); endHour = 18; }
    _travelStartHour = static_cast<uint8>(startHour);
    _travelEndHour = static_cast<uint8>(endHour);
    LOG_INFO("server.loading", "[LW Travel] Daily travel window configured for {:02}:00-{:02}:00 server-local time{}.",
        static_cast<uint32>(_travelStartHour), static_cast<uint32>(_travelEndHour),
        _travelStartHour == _travelEndHour ? " (24-hour travel)" : "");
}

uint8 TravelingEventManager::GetLocalHour() const
{
    std::time_t const now = std::time(nullptr);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    return static_cast<uint8>(localTime.tm_hour);
}

bool TravelingEventManager::IsTravelWindowOpen() const
{
    if (_travelStartHour == _travelEndHour) return true;
    uint8 const hour = GetLocalHour();
    if (_travelStartHour < _travelEndHour) return hour >= _travelStartHour && hour < _travelEndHour;
    return hour >= _travelStartHour || hour < _travelEndHour;
}

void TravelingEventManager::Reset()
{
    for (auto& [eventId, runtime] : _active) { (void)eventId; CleanupRuntime(runtime); }
    _active.clear();
    for (auto& [debugKey, runtime] : _debugCamps) { (void)debugKey; CleanupRuntime(runtime); }
    _debugCamps.clear();
    _campLayouts.clear();
    _campNodeZOverrides.clear();
    _definitions.clear();
}

void TravelingEventManager::LoadDefinitions()
{
    for (auto& [eventId, runtime] : _active) { (void)eventId; CleanupRuntime(runtime); }
    _active.clear();
    for (auto& [debugKey, runtime] : _debugCamps) { (void)debugKey; CleanupRuntime(runtime); }
    _debugCamps.clear();
    _campLayouts.clear();
    _campNodeZOverrides.clear();
    _definitions.clear();

    if (QueryResult result = WorldDatabase.Query("SELECT `id`,`name`,`traversal_mode`,`auto_start`,`enabled` FROM `lw_traveling_event` ORDER BY `id`"))
    {
        do
        {
            Field* f = result->Fetch();
            TravelingEventDefinition d;
            d.Id = f[0].Get<uint32>(); d.Name = f[1].Get<std::string>();
            d.TraversalMode = static_cast<TravelingTraversalMode>(f[2].Get<uint8>());
            d.AutoStart = f[3].Get<uint8>() != 0; d.Enabled = f[4].Get<uint8>() != 0;
            if (static_cast<uint8>(d.TraversalMode) > static_cast<uint8>(TravelingTraversalMode::OneWay))
            {
                LOG_ERROR("server.loading", "[LW Travel] Event {} ({}) has invalid traversal_mode {}; disabled in memory.", d.Id, d.Name, f[2].Get<uint32>());
                d.Enabled = false;
            }
            _definitions.emplace(d.Id, std::move(d));
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `id`,`event_id`,`member_order`,`member_key`,`creature_entry`,`is_leader`,`vendor_while_camped`,`enabled` FROM `lw_traveling_event_member` WHERE `enabled`=1 ORDER BY `event_id`,`member_order`,`id`"))
    {
        do
        {
            Field* f = result->Fetch(); uint32 const eventId = f[1].Get<uint32>(); auto it = _definitions.find(eventId); if (it == _definitions.end()) continue;
            TravelingMemberDefinition m; m.Id=f[0].Get<uint32>(); m.EventId=eventId; m.MemberOrder=f[2].Get<uint16>(); m.MemberKey=f[3].Get<std::string>();
            m.CreatureEntry=f[4].Get<uint32>(); m.IsLeader=f[5].Get<uint8>()!=0; m.VendorWhileCamped=f[6].Get<uint8>()!=0; m.Enabled=f[7].Get<uint8>()!=0;
            it->second.Members.push_back(std::move(m));
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `id`,`event_id`,`stop_order`,`route_node_id`,`camp_layout_id`,`dwell_seconds` FROM `lw_traveling_event_stop` WHERE `enabled`=1 ORDER BY `event_id`,`stop_order`,`id`"))
    {
        do
        {
            Field* f=result->Fetch(); uint32 const eventId=f[1].Get<uint32>(); auto it=_definitions.find(eventId); if(it==_definitions.end()) continue;
            TravelingStopDefinition st; st.Id=f[0].Get<uint32>(); st.EventId=eventId; st.StopOrder=f[2].Get<uint32>(); st.RouteNodeId=f[3].Get<uint32>(); st.CampLayoutId=f[4].Get<uint32>(); st.DwellSeconds=std::max<uint32>(1,f[5].Get<uint32>());
            it->second.Stops.push_back(std::move(st));
        } while(result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `id`,`event_id`,`from_stop_id`,`to_stop_id`,`speaker_member_id`,`departure_text`,`arrival_text`,`enabled` FROM `lw_traveling_event_leg` WHERE `enabled`=1 ORDER BY `event_id`,`id`"))
    {
        do
        {
            Field* f=result->Fetch(); uint32 const eventId=f[1].Get<uint32>(); auto it=_definitions.find(eventId); if(it==_definitions.end()) continue;
            TravelingLegDefinition leg; leg.Id=f[0].Get<uint32>(); leg.EventId=eventId; leg.FromStopId=f[2].Get<uint32>(); leg.ToStopId=f[3].Get<uint32>(); leg.SpeakerMemberId=f[4].Get<uint32>();
            leg.DepartureText=f[5].IsNull()?std::string():f[5].Get<std::string>(); leg.ArrivalText=f[6].IsNull()?std::string():f[6].Get<std::string>(); leg.Enabled=f[7].Get<uint8>()!=0;
            it->second.Legs.push_back(std::move(leg));
        } while(result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `id`,`name`,`enabled` FROM `lw_traveling_camp_layout` ORDER BY `id`"))
    {
        do { Field* f=result->Fetch(); TravelingCampLayoutDefinition l; l.Id=f[0].Get<uint32>(); l.Name=f[1].Get<std::string>(); l.Enabled=f[2].Get<uint8>()!=0; _campLayouts.emplace(l.Id,std::move(l)); } while(result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `id`,`layout_id`,`member_key`,`forward_offset`,`right_offset`,`z_offset`,`orientation_offset` FROM `lw_traveling_camp_layout_member` WHERE `enabled`=1 ORDER BY `layout_id`,`id`"))
    {
        do
        {
            Field* f=result->Fetch(); uint32 const layoutId=f[1].Get<uint32>(); auto it=_campLayouts.find(layoutId); if(it==_campLayouts.end()) continue;
            TravelingCampMemberPlacementDefinition p; p.Id=f[0].Get<uint32>(); p.LayoutId=layoutId; p.MemberKey=f[2].Get<std::string>(); p.ForwardOffset=f[3].Get<float>(); p.RightOffset=f[4].Get<float>(); p.ZOffset=f[5].Get<float>(); p.OrientationOffset=f[6].Get<float>(); it->second.Members.push_back(std::move(p));
        } while(result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `id`,`layout_id`,`gameobject_entry`,`forward_offset`,`right_offset`,`z_offset`,`orientation_offset` FROM `lw_traveling_camp_layout_prop` WHERE `enabled`=1 ORDER BY `layout_id`,`id`"))
    {
        do
        {
            Field* f=result->Fetch(); uint32 const layoutId=f[1].Get<uint32>(); auto it=_campLayouts.find(layoutId); if(it==_campLayouts.end()) continue;
            TravelingCampPropDefinition p; p.Id=f[0].Get<uint32>(); p.LayoutId=layoutId; p.GameObjectEntry=f[2].Get<uint32>(); p.ForwardOffset=f[3].Get<float>(); p.RightOffset=f[4].Get<float>(); p.ZOffset=f[5].Get<float>(); p.OrientationOffset=f[6].Get<float>(); it->second.Props.push_back(std::move(p));
        } while(result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `route_node_id`,`target_type`,`target_id`,`z_override` FROM `lw_traveling_camp_node_z_override` WHERE `enabled`=1 ORDER BY `route_node_id`,`target_type`,`target_id`"))
    {
        do
        {
            Field* f=result->Fetch(); TravelingCampNodeZOverride row; row.RouteNodeId=f[0].Get<uint32>(); row.TargetType=static_cast<TravelingCampTargetType>(f[1].Get<uint8>()); row.TargetId=f[2].Get<uint32>(); row.ZOverride=f[3].Get<float>();
            uint64 const key=(static_cast<uint64>(row.RouteNodeId)<<32)|(static_cast<uint64>(static_cast<uint8>(row.TargetType))<<24)|static_cast<uint64>(row.TargetId&0x00FFFFFF); _campNodeZOverrides[key]=row;
        } while(result->NextRow());
    }

    for (auto& [eventId,d] : _definitions)
    {
        uint32 leaders=0; for(auto const& m:d.Members) if(m.IsLeader) ++leaders;
        if(d.Enabled && (d.Members.empty() || leaders!=1 || d.Stops.size()<2)) { LOG_ERROR("server.loading","[LW Travel] Event {} ({}) invalid: members={}, leaders={}, stops={}; disabled in memory.",eventId,d.Name,d.Members.size(),leaders,d.Stops.size()); d.Enabled=false; }
    }
    LOG_INFO("server.loading","[LW Travel] Loaded {} traveling-world-event definition(s), {} reusable camp layout(s), and {} camp-node Z override(s).",_definitions.size(),_campLayouts.size(),_campNodeZOverrides.size());
}

void TravelingEventManager::StartAutoEvents()
{
    for(auto const& [eventId,d]:_definitions)
    {
        if(!d.Enabled || !d.AutoStart) continue;
        std::string error; auto const result=Start(eventId,&error);
        if(result!=TravelingEventStartResult::Started && result!=TravelingEventStartResult::AlreadyActive)
            LOG_ERROR("server.loading","[LW Travel] Auto-start for event {} ({}) failed: {}.",eventId,d.Name,error);
    }
}

TravelingEventDefinition const* TravelingEventManager::GetDefinition(uint32 id) const { auto it=_definitions.find(id); return it==_definitions.end()?nullptr:&it->second; }
TravelingCampLayoutDefinition const* TravelingEventManager::GetCampLayout(uint32 id) const { auto it=_campLayouts.find(id); return it==_campLayouts.end()||!it->second.Enabled?nullptr:&it->second; }
TravelingMemberDefinition const* TravelingEventManager::GetLeaderDefinition(TravelingEventDefinition const& d) const { for(auto const& m:d.Members) if(m.IsLeader) return &m; return nullptr; }
TravelingMemberDefinition const* TravelingEventManager::GetMemberDefinition(TravelingEventDefinition const& d,uint32 id) const { for(auto const& m:d.Members) if(m.Id==id) return &m; return nullptr; }
TravelingMemberDefinition const* TravelingEventManager::GetMemberDefinitionByKey(TravelingEventDefinition const& d,std::string const& key) const { for(auto const& m:d.Members) if(m.MemberKey==key) return &m; return nullptr; }
TravelingLegDefinition const* TravelingEventManager::GetLeg(TravelingEventDefinition const& d,uint32 from,uint32 to) const { for(auto const& l:d.Legs) if(l.Enabled&&l.FromStopId==from&&l.ToStopId==to) return &l; return nullptr; }
TravelingLegDefinition const* TravelingEventManager::GetLegById(TravelingEventDefinition const& d,uint32 id) const { if(!id)return nullptr; for(auto const& l:d.Legs) if(l.Enabled&&l.Id==id)return &l; return nullptr; }

float TravelingEventManager::GetCampNodeZOverride(uint32 node,TravelingCampTargetType type,uint32 target) const
{
    uint64 const key=(static_cast<uint64>(node)<<32)|(static_cast<uint64>(static_cast<uint8>(type))<<24)|static_cast<uint64>(target&0x00FFFFFF); auto it=_campNodeZOverrides.find(key); return it==_campNodeZOverrides.end()?0.0f:it->second.ZOverride;
}

Creature* TravelingEventManager::GetCreature(uint16 mapId,ObjectGuid guid) const { if(guid.IsEmpty())return nullptr; Map* map=sMapMgr->FindMap(mapId,0); return map?map->GetCreature(guid):nullptr; }
ObjectGuid TravelingEventManager::GetMemberGuid(ActiveTravelingEvent const& r,uint32 id) const { auto it=r.MemberGuids.find(id); return it==r.MemberGuids.end()?ObjectGuid::Empty:it->second; }
Creature* TravelingEventManager::GetMemberCreature(ActiveTravelingEvent const& r,TravelingMemberDefinition const& m) const { return GetCreature(r.MapId,GetMemberGuid(r,m.Id)); }
Creature* TravelingEventManager::GetLegSpeaker(ActiveTravelingEvent const& r,TravelingEventDefinition const& d,TravelingLegDefinition const* leg) const
{
    if(!leg || leg->SpeakerMemberId==0) return GetCreature(r.MapId,r.LeaderGuid);
    auto const* m=GetMemberDefinition(d,leg->SpeakerMemberId); return m?GetMemberCreature(r,*m):nullptr;
}

void TravelingEventManager::ApplyProtectedState(Creature* c) const { if(!c)return; c->CombatStop(true); c->SetReactState(REACT_PASSIVE); c->SetImmuneToAll(true); c->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE); }
void TravelingEventManager::ApplyTravelServices(ActiveTravelingEvent& r,TravelingEventDefinition const& d) const { for(auto const& m:d.Members) if(Creature* c=GetMemberCreature(r,m)){ ApplyProtectedState(c); if(m.VendorWhileCamped)c->RemoveNpcFlag(UNIT_NPC_FLAG_VENDOR); } }
void TravelingEventManager::ApplyCampServices(ActiveTravelingEvent& r,TravelingEventDefinition const& d) const { for(auto const& m:d.Members) if(Creature* c=GetMemberCreature(r,m)){ ApplyProtectedState(c); if(m.VendorWhileCamped)c->SetNpcFlag(UNIT_NPC_FLAG_VENDOR); } }

bool TravelingEventManager::SpawnMembersAtStop(TravelingEventDefinition const& d,uint32 stopIndex,ActiveTravelingEvent& r,bool createGroup,std::string* error)
{
    if(stopIndex>=d.Stops.size()){if(error)*error="requested start stop is out of range";return false;}
    if(!GetLeaderDefinition(d)||d.Members.empty()){if(error)*error="traveling event requires exactly one enabled leader and at least one member";return false;}
    auto const& stop=d.Stops[stopIndex]; auto const* node=sLivingWorldDataMgr.GetRouteNode(stop.RouteNodeId); if(!node||!node->Enabled){if(error)*error="start stop references a missing/disabled route node";return false;}
    Map* map=sMapMgr->FindMap(node->MapId,0); if(!map) map=sMapMgr->CreateBaseMap(node->MapId); if(!map){if(error)*error="start map could not be created";return false;}
    Position pos; pos.Relocate(node->X,node->Y,node->Z,node->Orientation);
    r={}; r.EventId=d.Id; r.StopIndex=stopIndex; r.TravelDirection=1; r.State=TravelingEventState::Camped; r.MapId=node->MapId;
    RuntimeEntityGroup* group=nullptr; if(createGroup){ RuntimeEntityGroup& g=sRuntimeEntityGroupMgr.CreateGroup(0,0); g.RouteFormation=RouteFormationProfile::TravelingCaravan; r.RuntimeGroupId=g.Id; group=&g; }
    for(auto const& m:d.Members)
    {
        if(m.CreatureEntry==0){if(error)*error="traveling event member has creature_entry=0";CleanupRuntime(r);return false;}
        TempSummon* summon=map->SummonCreature(m.CreatureEntry,pos,nullptr,0); if(!summon){if(error)*error="failed to summon traveling event member";CleanupRuntime(r);return false;}
        ApplyProtectedState(summon); if(m.VendorWhileCamped)summon->RemoveNpcFlag(UNIT_NPC_FLAG_VENDOR); r.MemberGuids[m.Id]=summon->GetGUID(); if(m.IsLeader)r.LeaderGuid=summon->GetGUID();
        if(group){ RuntimeEntity e; e.EntityType=static_cast<uint8>(EntityProviderType::Creature); e.MapId=node->MapId; e.MemberId=m.Id; e.Entry=m.CreatureEntry; e.TacticalRole=static_cast<uint8>(m.IsLeader?TacticalRole::Commander:TacticalRole::Default); e.Guid=summon->GetGUID(); group->Entities.push_back(std::move(e)); }
    }
    if(r.LeaderGuid.IsEmpty()){if(error)*error="traveling event did not produce a leader creature";CleanupRuntime(r);return false;}
    LOG_INFO("server.loading","[LW Travel] Event {} spawned {} member(s) at route node {}{}.",d.Id,r.MemberGuids.size(),stop.RouteNodeId,createGroup?" with a traveling-caravan movement group":" for debug camp materialization"); return true;
}

TravelingEventStartResult TravelingEventManager::Start(uint32 eventId,std::string* error)
{
    if(_active.find(eventId)!=_active.end()){if(error)*error="event is already active";return TravelingEventStartResult::AlreadyActive;}
    auto const* d=GetDefinition(eventId); if(!d){if(error)*error="event is not loaded; check lw_traveling_event and run .lw reload";return TravelingEventStartResult::NotFound;} if(!d->Enabled){if(error)*error="event is loaded but disabled";return TravelingEventStartResult::Disabled;}
    if(d->Stops.size()<2||d->Members.empty()||!GetLeaderDefinition(*d)){if(error)*error="invalid configuration: event needs at least two stops, members, and exactly one leader";return TravelingEventStartResult::InvalidConfiguration;}
    ActiveTravelingEvent r; std::string spawnError; if(!SpawnMembersAtStop(*d,0,r,true,&spawnError)){if(error)*error=spawnError;return TravelingEventStartResult::SpawnFailed;}
    if(!BeginCamp(r,*d)){CleanupRuntime(r);if(error)*error="members spawned, but the initial camp could not be established";return TravelingEventStartResult::SpawnFailed;}
    _active.emplace(eventId,std::move(r)); LOG_INFO("server.loading","[LW Travel] Started event {} ({}) traversal={} auto_start={}.",d->Id,d->Name,static_cast<uint32>(d->TraversalMode),d->AutoStart?1:0); return TravelingEventStartResult::Started;
}

void TravelingEventManager::CleanupRuntime(ActiveTravelingEvent& r)
{
    if(r.RuntimeGroupId!=0)sMovementController.CancelGroup(r.RuntimeGroupId); Map* map=sMapMgr->FindMap(r.MapId,0); if(map)for(auto const& guid:r.CampPropGuids)if(GameObject* go=map->GetGameObject(guid))go->Delete(); r.CampPropGuids.clear();
    for(auto const& [id,guid]:r.MemberGuids){(void)id;if(Creature* c=GetCreature(r.MapId,guid))if(TempSummon* s=c->ToTempSummon())s->DespawnOrUnsummon();} r.MemberGuids.clear(); r.LeaderGuid=ObjectGuid::Empty;
    if(r.RuntimeGroupId!=0)sRuntimeEntityGroupMgr.RemoveGroup(r.RuntimeGroupId); r.RuntimeGroupId=0;
}

uint64 TravelingEventManager::MakeDebugCampKey(uint32 eventId,uint32 nodeId) const { return (static_cast<uint64>(eventId)<<32)|nodeId; }
bool TravelingEventManager::DebugDespawnCamp(uint32 eventId,uint32 nodeId,std::string* error){auto it=_debugCamps.find(MakeDebugCampKey(eventId,nodeId));if(it==_debugCamps.end()){if(error)*error="no debug camp is spawned for that event/node";return false;}CleanupRuntime(it->second);_debugCamps.erase(it);return true;}
bool TravelingEventManager::DebugSpawnCamp(uint32 eventId,uint32 nodeId,std::string* error)
{
    auto const* d=GetDefinition(eventId);if(!d){if(error)*error="event is not loaded";return false;}if(!d->Enabled){if(error)*error="event is loaded but disabled";return false;}
    uint32 idx=0;bool found=false;for(uint32 i=0;i<d->Stops.size();++i)if(d->Stops[i].RouteNodeId==nodeId){idx=i;found=true;break;}if(!found){if(error)*error="route node is not configured as a stop for this traveling event";return false;}
    auto const& stop=d->Stops[idx];if(stop.CampLayoutId==0||!GetCampLayout(stop.CampLayoutId)){if(error)*error="stop has no enabled camp layout";return false;}
    uint64 const key=MakeDebugCampKey(eventId,nodeId);auto old=_debugCamps.find(key);if(old!=_debugCamps.end()){CleanupRuntime(old->second);_debugCamps.erase(old);}ActiveTravelingEvent r;if(!SpawnMembersAtStop(*d,idx,r,false,error))return false;if(!BeginCamp(r,*d)){CleanupRuntime(r);if(error)*error="failed to materialize camp layout at the requested node";return false;}r.StateTimerMs=0;_debugCamps.emplace(key,std::move(r));return true;
}

bool TravelingEventManager::Stop(uint32 eventId,std::string* error){auto it=_active.find(eventId);if(it==_active.end()){if(error)*error="event is not active";return false;}CleanupRuntime(it->second);_active.erase(it);LOG_INFO("server.loading","[LW Travel] Stopped event {}.",eventId);return true;}

bool TravelingEventManager::ResolveNextStopIndex(ActiveTravelingEvent& r,TravelingEventDefinition const& d,uint32& to)
{
    if(d.Stops.size()<2||r.StopIndex>=d.Stops.size())return false;
    switch(d.TraversalMode)
    {
        case TravelingTraversalMode::Loop: to=(r.StopIndex+1)%d.Stops.size();return true;
        case TravelingTraversalMode::PingPong:
            if(r.TravelDirection>=0&&r.StopIndex+1>=d.Stops.size())r.TravelDirection=-1;else if(r.TravelDirection<0&&r.StopIndex==0)r.TravelDirection=1;
            {int64 const candidate=static_cast<int64>(r.StopIndex)+r.TravelDirection;if(candidate<0||candidate>=static_cast<int64>(d.Stops.size()))return false;to=static_cast<uint32>(candidate);return true;}
        case TravelingTraversalMode::OneWay: if(r.StopIndex+1>=d.Stops.size()){r.JourneyComplete=true;return false;}to=r.StopIndex+1;return true;
    }
    return false;
}

bool TravelingEventManager::BeginTravel(ActiveTravelingEvent& r,TravelingEventDefinition const& d)
{
    uint32 toIndex=0;if(!ResolveNextStopIndex(r,d,toIndex))return r.JourneyComplete;uint32 const fromIndex=r.StopIndex;auto const& from=d.Stops[fromIndex];auto const& to=d.Stops[toIndex];auto const* leg=GetLeg(d,from.Id,to.Id);
    if(!GetCreature(r.MapId,r.LeaderGuid))return false;for(auto const& m:d.Members)if(!GetMemberCreature(r,m))return false;EndCamp(r,d);ApplyTravelServices(r,d);
    if(leg&&!leg->DepartureText.empty())if(Creature* speaker=GetLegSpeaker(r,d,leg))speaker->Say(leg->DepartureText,LANG_UNIVERSAL);
    if(!sMovementController.StartRouteJourney(r.RuntimeGroupId,from.RouteNodeId,to.RouteNodeId)){LOG_ERROR("server.loading","[LW Travel] Event {} could not start route journey {} -> {}.",d.Id,from.RouteNodeId,to.RouteNodeId);return false;}
    r.StopIndex=toIndex;r.ActiveLegId=leg?leg->Id:0;r.State=TravelingEventState::Traveling;r.StateTimerMs=0;LOG_INFO("server.loading","[LW Travel] Event {} departed stop {} for stop {} using leg {} with {} member(s).",d.Id,from.Id,to.Id,r.ActiveLegId,r.MemberGuids.size());return true;
}

void TravelingEventManager::EndCamp(ActiveTravelingEvent& r,TravelingEventDefinition const& d){Map* map=sMapMgr->FindMap(r.MapId,0);if(map)for(auto const& guid:r.CampPropGuids)if(GameObject* go=map->GetGameObject(guid))go->Delete();r.CampPropGuids.clear();ApplyTravelServices(r,d);}

bool TravelingEventManager::BeginCamp(ActiveTravelingEvent& r,TravelingEventDefinition const& d)
{
    if(r.StopIndex>=d.Stops.size())return false;auto const& stop=d.Stops[r.StopIndex];auto const* node=sLivingWorldDataMgr.GetRouteNode(stop.RouteNodeId);if(!node||!node->Enabled)return false;r.MapId=node->MapId;for(auto const& m:d.Members)if(!GetMemberCreature(r,m))return false;
    auto const* layout=stop.CampLayoutId?GetCampLayout(stop.CampLayoutId):nullptr;if(stop.CampLayoutId&&!layout){LOG_ERROR("server.loading","[LW Travel] Event {} stop {} references missing/disabled camp layout {}.",d.Id,stop.Id,stop.CampLayoutId);return false;}
    auto build=[node](float forward,float right,float zoff,float ooff,float& x,float& y,float& z,float& o){float const c=std::cos(node->Orientation),s=std::sin(node->Orientation);x=node->X+c*forward-s*right;y=node->Y+s*forward+c*right;z=node->Z+zoff;o=node->Orientation+ooff;};
    if(layout)for(auto const& p:layout->Members){auto const* m=GetMemberDefinitionByKey(d,p.MemberKey);if(!m){LOG_WARN("server.loading","[LW Travel] Event {} layout {} placement {} references unknown member_key '{}'.",d.Id,layout->Id,p.Id,p.MemberKey);continue;}Creature* c=GetMemberCreature(r,*m);if(!c)return false;float x,y,z,o;build(p.ForwardOffset,p.RightOffset,p.ZOffset+GetCampNodeZOverride(stop.RouteNodeId,TravelingCampTargetType::MemberPlacement,p.Id),p.OrientationOffset,x,y,z,o);c->NearTeleportTo(x,y,z,o);ApplyProtectedState(c);}
    ApplyCampServices(r,d);
    if(auto const* leg=GetLegById(d,r.ActiveLegId))if(!leg->ArrivalText.empty())if(Creature* speaker=GetLegSpeaker(r,d,leg))speaker->Say(leg->ArrivalText,LANG_UNIVERSAL);
    Map* map=sMapMgr->FindMap(node->MapId,0);if(map&&layout)for(auto const& p:layout->Props){if(!p.GameObjectEntry)continue;float x,y,z,o;build(p.ForwardOffset,p.RightOffset,p.ZOffset+GetCampNodeZOverride(stop.RouteNodeId,TravelingCampTargetType::LayoutProp,p.Id),p.OrientationOffset,x,y,z,o);Position pos;pos.Relocate(x,y,z,o);if(GameObject* go=map->SummonGameObject(p.GameObjectEntry,pos,0,0,0,0,0,true))r.CampPropGuids.push_back(go->GetGUID());}
    r.State=TravelingEventState::Camped;r.StateTimerMs=std::max<uint32>(1,stop.DwellSeconds)*IN_MILLISECONDS;LOG_INFO("server.loading","[LW Travel] Event {} camped at stop {} route node {} for {} second(s); layout={}, members={}, props={}, leg={}.",d.Id,stop.Id,stop.RouteNodeId,stop.DwellSeconds,stop.CampLayoutId,r.MemberGuids.size(),r.CampPropGuids.size(),r.ActiveLegId);return true;
}

void TravelingEventManager::Update(uint32 diff)
{
    for(auto it=_active.begin();it!=_active.end();)
    {
        auto& r=it->second;auto const* d=GetDefinition(r.EventId);if(!d){CleanupRuntime(r);it=_active.erase(it);continue;}bool missing=false;for(auto const& m:d->Members)if(!GetMemberCreature(r,m)){missing=true;break;}if(missing||r.MemberGuids.size()!=d->Members.size()){LOG_ERROR("server.loading","[LW Travel] Event {} lost one or more traveling members; stopping runtime.",r.EventId);CleanupRuntime(r);it=_active.erase(it);continue;}
        if(r.State==TravelingEventState::Traveling){ApplyTravelServices(r,*d);if(!sMovementController.IsGroupMoving(r.RuntimeGroupId)&&!BeginCamp(r,*d)){LOG_ERROR("server.loading","[LW Travel] Event {} failed to establish stop state; stopping runtime.",r.EventId);CleanupRuntime(r);it=_active.erase(it);continue;}}
        else{ApplyCampServices(r,*d);if(r.JourneyComplete){++it;continue;}if(!IsTravelWindowOpen())r.StateTimerMs=0;else if(r.StateTimerMs>diff)r.StateTimerMs-=diff;else if(!BeginTravel(r,*d)){LOG_ERROR("server.loading","[LW Travel] Event {} failed to depart stop; stopping runtime.",r.EventId);CleanupRuntime(r);it=_active.erase(it);continue;}}
        ++it;
    }
}

std::string TravelingEventManager::BuildStatusReport() const
{
    std::ostringstream out;uint8 const hour=GetLocalHour();out<<"\nTraveling world events: "<<_active.size()<<" active / "<<_definitions.size()<<" loaded\n"<<"Travel window: "<<static_cast<uint32>(_travelStartHour)<<":00-"<<static_cast<uint32>(_travelEndHour)<<":00 server-local | current hour="<<static_cast<uint32>(hour)<<" | "<<(IsTravelWindowOpen()?"OPEN":"CLOSED")<<"\n";
    for(auto const& [id,r]:_active){auto const* d=GetDefinition(id);char const* mode="UNKNOWN";if(d)switch(d->TraversalMode){case TravelingTraversalMode::Loop:mode="LOOP";break;case TravelingTraversalMode::PingPong:mode="PING_PONG";break;case TravelingTraversalMode::OneWay:mode="ONE_WAY";break;}out<<"  #"<<id<<" "<<(d?d->Name:"<missing>")<<" state="<<(r.State==TravelingEventState::Traveling?"TRAVELING":"CAMPED")<<" traversal="<<mode<<" stopIndex="<<r.StopIndex<<" direction="<<(r.TravelDirection>=0?"forward":"reverse")<<" completed="<<(r.JourneyComplete?"yes":"no")<<" movementGroup="<<r.RuntimeGroupId<<" members="<<r.MemberGuids.size()<<" leaderGuid="<<r.LeaderGuid.ToString()<<"\n";}
    return out.str();
}
}
