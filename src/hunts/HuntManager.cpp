#include "HuntManager.h"

#include "Chat.h"
#include "Creature.h"
#include "GameObject.h"
#include "LwCreatureTemplateManager.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "Random.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "CreatureAI.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace lw
{
HuntManager& HuntManager::Instance()
{
    static HuntManager instance;
    return instance;
}

void HuntManager::Configure(bool enabled, uint8 minimumLevel, float xpMultiplier, bool debug)
{
    _enabled = enabled;
    _minimumLevel = std::max<uint8>(1, minimumLevel);
    _xpMultiplier = std::max(0.0f, xpMultiplier);
    _debug = debug;
}

void HuntManager::Reset()
{
    _hunts.clear();
    _zones.clear();
    _finalLocations.clear();
    _giverEntries.clear();
    _runtimes.clear();
    _updateTimerMs = 0;
}

void HuntManager::LoadDefinitions()
{
    _hunts.clear();
    _zones.clear();
    _finalLocations.clear();
    _giverEntries.clear();

    if (!_enabled)
        return;

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`,`name`,`min_level`,`max_level`,`prey_creature_entry`,`prey_lw_template_id`,`activation_gameobject_entry`,`ambush_health_multiplier`,`final_health_multiplier`,`escape_health_pct`,`ambush_count`,`enabled` FROM `lw_hunt` WHERE `enabled`=1"))
    {
        do
        {
            Field* f = result->Fetch();
            HuntDefinition d;
            d.Id=f[0].Get<uint32>(); d.Name=f[1].Get<std::string>(); d.MinLevel=f[2].Get<uint8>(); d.MaxLevel=f[3].Get<uint8>();
            d.PreyCreatureEntry=f[4].Get<uint32>(); d.PreyLwTemplateId=f[5].Get<uint32>(); d.ActivationGameObjectEntry=f[6].Get<uint32>();
            d.AmbushHealthMultiplier=f[7].Get<float>(); d.FinalHealthMultiplier=f[8].Get<float>();
            d.EscapeHealthPct=f[9].Get<uint8>(); d.AmbushCount=f[10].Get<uint8>(); d.Enabled=f[11].Get<uint8>()!=0;
            _hunts[d.Id]=d;
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`,`hunt_id`,`zone_id`,`min_level`,`max_level`,`weight`,`enabled` FROM `lw_hunt_zone` WHERE `enabled`=1"))
    {
        do
        {
            Field* f=result->Fetch(); HuntZoneDefinition d;
            d.Id=f[0].Get<uint32>(); d.HuntId=f[1].Get<uint32>(); d.ZoneId=f[2].Get<uint32>(); d.MinLevel=f[3].Get<uint8>(); d.MaxLevel=f[4].Get<uint8>(); d.Weight=f[5].Get<uint32>(); d.Enabled=f[6].Get<uint8>()!=0;
            _zones.push_back(d);
        } while(result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query(
        "SELECT `id`,`hunt_id`,`zone_id`,`map_id`,`x`,`y`,`z`,`orientation`,`location_name`,`weight`,`enabled` FROM `lw_hunt_final_location` WHERE `enabled`=1"))
    {
        do
        {
            Field* f=result->Fetch(); HuntFinalLocationDefinition d;
            d.Id=f[0].Get<uint32>(); d.HuntId=f[1].Get<uint32>(); d.ZoneId=f[2].Get<uint32>(); d.MapId=f[3].Get<uint16>(); d.X=f[4].Get<float>(); d.Y=f[5].Get<float>(); d.Z=f[6].Get<float>(); d.Orientation=f[7].Get<float>(); d.LocationName=f[8].Get<std::string>(); d.Weight=f[9].Get<uint32>(); d.Enabled=f[10].Get<uint8>()!=0;
            _finalLocations.push_back(d);
        } while(result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `creature_entry`,`id` FROM `lw_hunt_giver` WHERE `enabled`=1"))
    {
        do { Field* f=result->Fetch(); _giverEntries[f[0].Get<uint32>()]=f[1].Get<uint32>(); } while(result->NextRow());
    }

    LOG_INFO("server.loading", "[LW Hunt] Loaded {} hunt(s), {} zone rule(s), {} final location(s), and {} hunt giver entry(s).", _hunts.size(), _zones.size(), _finalLocations.size(), _giverEntries.size());
}

void HuntManager::Initialize()
{
    if (_enabled)
        LoadRuntimes();
}

void HuntManager::LoadRuntimes()
{
    _runtimes.clear();
    if (QueryResult result = CharacterDatabase.Query("SELECT `guid`,`hunt_id`,`giver_entry`,`giver_spawn_id`,`zone_id`,`final_location_id`,`tracking_progress`,`ambushes_completed`,`state` FROM `lw_hunt_runtime`"))
    {
        do
        {
            Field* f=result->Fetch(); HuntRuntime r;
            r.CharacterGuid=f[0].Get<uint32>(); r.HuntId=f[1].Get<uint32>(); r.GiverEntry=f[2].Get<uint32>(); r.GiverSpawnId=f[3].Get<uint32>(); r.ZoneId=f[4].Get<uint32>(); r.FinalLocationId=f[5].Get<uint32>(); r.TrackingProgress=f[6].Get<uint8>(); r.AmbushesCompleted=f[7].Get<uint8>(); r.State=static_cast<HuntState>(f[8].Get<uint8>());
            _runtimes[r.CharacterGuid]=r;
        } while(result->NextRow());
    }
    LOG_INFO("server.loading", "[LW Hunt] Restored {} active hunt runtime(s).", _runtimes.size());
}

void HuntManager::SaveRuntime(HuntRuntime const& r)
{
    CharacterDatabase.Execute("REPLACE INTO `lw_hunt_runtime` (`guid`,`hunt_id`,`giver_entry`,`giver_spawn_id`,`zone_id`,`final_location_id`,`tracking_progress`,`ambushes_completed`,`state`) VALUES ({},{},{},{},{},{},{},{},{})",
        r.CharacterGuid,r.HuntId,r.GiverEntry,r.GiverSpawnId,r.ZoneId,r.FinalLocationId,r.TrackingProgress,r.AmbushesCompleted,static_cast<uint32>(r.State));
}

void HuntManager::DeleteRuntime(uint32 guid)
{
    CharacterDatabase.Execute("DELETE FROM `lw_hunt_runtime` WHERE `guid`={}", guid);
    _runtimes.erase(guid);
}

bool HuntManager::HasActiveHunt(Player const* player) const { return GetRuntime(player)!=nullptr; }
HuntRuntime const* HuntManager::GetRuntime(Player const* player) const
{
    if(!player) return nullptr; auto it=_runtimes.find(player->GetGUID().GetCounter()); return it==_runtimes.end()?nullptr:&it->second;
}
HuntDefinition const* HuntManager::GetDefinition(uint32 id) const { auto it=_hunts.find(id); return it==_hunts.end()?nullptr:&it->second; }
bool HuntManager::IsHuntGiver(uint32 entry) const { return _giverEntries.find(entry)!=_giverEntries.end(); }

HuntZoneDefinition const* HuntManager::SelectZone(HuntDefinition const& hunt, uint8 level) const
{
    std::vector<HuntZoneDefinition const*> eligible;
    for(auto const& z:_zones) if(z.HuntId==hunt.Id && z.Enabled && level>=z.MinLevel && level<=z.MaxLevel) eligible.push_back(&z);
    if(eligible.empty()) return nullptr;
    return eligible[urand(0, static_cast<uint32>(eligible.size()-1))];
}

HuntFinalLocationDefinition const* HuntManager::SelectFinalLocation(HuntRuntime const& runtime) const
{
    std::vector<HuntFinalLocationDefinition const*> eligible;
    for(auto const& l:_finalLocations) if(l.HuntId==runtime.HuntId && l.ZoneId==runtime.ZoneId && l.Enabled) eligible.push_back(&l);
    if(eligible.empty()) return nullptr;
    return eligible[urand(0, static_cast<uint32>(eligible.size()-1))];
}

bool HuntManager::RequestHunt(Player* player, Creature* giver, std::string& message)
{
    if(!_enabled){message="The Hunt system is disabled.";return false;}
    if(!player||!giver||!IsHuntGiver(giver->GetEntry())){message="That creature is not a Living World Huntmaster.";return false;}
    if(player->GetLevel()<_minimumLevel){message="You must be at least level "+std::to_string(_minimumLevel)+" to take a hunt.";return false;}
    if(HasActiveHunt(player)){message="You already have an active hunt.";return false;}

    std::vector<HuntDefinition const*> eligible;
    for(auto const& [id,h]:_hunts) if(h.Enabled && player->GetLevel()>=h.MinLevel && player->GetLevel()<=h.MaxLevel && SelectZone(h,player->GetLevel())) eligible.push_back(&h);
    if(eligible.empty()){message="I have no suitable prey for you right now.";return false;}
    HuntDefinition const& hunt=*eligible[urand(0,static_cast<uint32>(eligible.size()-1))];
    HuntZoneDefinition const* zone=SelectZone(hunt,player->GetLevel()); if(!zone){message="No suitable hunting ground was found.";return false;}

    HuntRuntime r; r.CharacterGuid=player->GetGUID().GetCounter(); r.HuntId=hunt.Id; r.GiverEntry=giver->GetEntry(); r.GiverSpawnId=giver->GetSpawnId(); r.ZoneId=zone->ZoneId; r.State=HuntState::Tracking;
    _runtimes[r.CharacterGuid]=r; SaveRuntime(r);
    message="Your quarry is "+hunt.Name+". Search Elwynn Forest and hunt normally; signs of your prey will reveal themselves.";
    return true;
}

bool HuntManager::AbandonHunt(Player* player, std::string& message)
{
    if(!player||!HasActiveHunt(player)){message="You do not have an active hunt.";return false;}
    auto it=_runtimes.find(player->GetGUID().GetCounter());
    if(it!=_runtimes.end()) RemoveFinalActivator(player,it->second);
    DeleteRuntime(player->GetGUID().GetCounter()); message="Your hunt has been abandoned."; return true;
}

bool HuntManager::TurnInHunt(Player* player, Creature* giver, std::string& message)
{
    if(!player||!giver){message="Invalid hunt turn-in.";return false;}
    auto it=_runtimes.find(player->GetGUID().GetCounter()); if(it==_runtimes.end()){message="You have no hunt to turn in.";return false;}
    HuntRuntime const& r=it->second;
    if(r.State!=HuntState::ReadyToTurnIn){message="Your quarry still lives.";return false;}
    if(r.GiverEntry!=giver->GetEntry() || (r.GiverSpawnId && r.GiverSpawnId!=giver->GetSpawnId())){message="Return to the Huntmaster who gave you this hunt.";return false;}
    HuntRuntime& mutableRuntime=it->second; RemoveFinalActivator(player,mutableRuntime);
    DeleteRuntime(r.CharacterGuid); message="A fine hunt. Your reward system will be added after the encounter loop is proven."; return true;
}

uint8 HuntManager::GetNextAmbushThreshold(HuntRuntime const& r, HuntDefinition const& h) const
{
    if(r.AmbushesCompleted>=h.AmbushCount || h.AmbushCount==0) return 100;
    return static_cast<uint8>((100u*(r.AmbushesCompleted+1u))/(h.AmbushCount+1u));
}

void HuntManager::OnCreatureKill(Player* player, Creature* killed)
{
    if(!_enabled||!player||!killed) return;
    auto it=_runtimes.find(player->GetGUID().GetCounter()); if(it==_runtimes.end()) return;
    HuntRuntime& r=it->second;

    if(r.ActivePreyGuid==killed->GetGUID())
    {
        r.ActivePreyGuid.Clear();
        if(r.ActivePreyFinal)
        {
            r.ActivePreyFinal=false; r.State=HuntState::ReadyToTurnIn; SaveRuntime(r);
            ChatHandler(player->GetSession()).SendSysMessage("|cff00ff00[LW Hunt]|r Your quarry is dead. Return to the Huntmaster who gave you the contract.");
        }
        return;
    }

    if(r.State!=HuntState::Tracking || player->GetZoneId()!=r.ZoneId) return;
    std::string ignored; AddProgress(player, static_cast<uint8>(urand(3,7)), ignored);
}

bool HuntManager::AddProgress(Player* player, uint8 amount, std::string& message)
{
    if(!player){message="Player required.";return false;}
    auto it=_runtimes.find(player->GetGUID().GetCounter()); if(it==_runtimes.end()){message="You have no active hunt.";return false;}
    HuntRuntime& r=it->second; if(r.State!=HuntState::Tracking){message="Tracking is already complete.";return false;}
    HuntDefinition const* h=GetDefinition(r.HuntId); if(!h){message="Hunt definition is missing.";return false;}
    uint8 old=r.TrackingProgress; r.TrackingProgress=static_cast<uint8>(std::min<uint32>(100, r.TrackingProgress+amount));
    uint8 threshold=GetNextAmbushThreshold(r,*h);
    SaveRuntime(r);
    if(r.TrackingProgress>=100){LocateFinal(player,r); message="Tracking reached 100%.";return true;}
    if(old<threshold && r.TrackingProgress>=threshold && r.AmbushesCompleted<h->AmbushCount)
    {
        std::string ambush; SpawnPrey(player,r,false,ambush); message=ambush; return true;
    }
    message="Tracking progress: "+std::to_string(r.TrackingProgress)+"%."; return true;
}

uint32 HuntManager::ResolvePreyEntry(HuntDefinition const& hunt) const
{
    if (hunt.PreyLwTemplateId)
        return sLwCreatureTemplateMgr.ResolveEntry(hunt.PreyLwTemplateId);
    return hunt.PreyCreatureEntry;
}

HuntFinalLocationDefinition const* HuntManager::GetFinalLocation(uint32 finalLocationId) const
{
    for (auto const& location : _finalLocations)
        if (location.Id == finalLocationId)
            return &location;
    return nullptr;
}

void HuntManager::RemoveFinalActivator(Player* player, HuntRuntime& r)
{
    if (r.FinalActivatorGuid.IsEmpty())
        return;

    if (player)
        if (GameObject* go = ObjectAccessor::GetGameObject(*player, r.FinalActivatorGuid))
            go->Delete();

    r.FinalActivatorGuid.Clear();
}

bool HuntManager::EnsureFinalActivator(Player* player, HuntRuntime& r)
{
    if (!player || r.State != HuntState::FinalLocated || !r.FinalLocationId)
        return false;

    HuntDefinition const* hunt = GetDefinition(r.HuntId);
    HuntFinalLocationDefinition const* location = GetFinalLocation(r.FinalLocationId);
    if (!hunt || !location || !hunt->ActivationGameObjectEntry)
        return false;

    if (!r.FinalActivatorGuid.IsEmpty())
    {
        if (GameObject* existing = ObjectAccessor::GetGameObject(*player, r.FinalActivatorGuid))
            if (existing->IsInWorld())
                return true;
        r.FinalActivatorGuid.Clear();
    }

    if (player->GetMapId() != location->MapId)
        return false;

    float const dx = player->GetPositionX() - location->X;
    float const dy = player->GetPositionY() - location->Y;
    if ((dx * dx + dy * dy) > (180.0f * 180.0f))
        return false;

    float z = location->Z;
    if (Map* map = player->GetMap())
    {
        float const groundZ = map->GetHeight(location->X, location->Y, z + 10.0f, true, 50.0f);
        if (groundZ > INVALID_HEIGHT)
            z = groundZ + 0.15f;
    }

    GameObject* activator = player->SummonGameObject(
        hunt->ActivationGameObjectEntry,
        location->X, location->Y, z, location->Orientation,
        0.0f, 0.0f,
        std::sin(location->Orientation * 0.5f),
        std::cos(location->Orientation * 0.5f),
        3600);

    if (!activator)
    {
        LOG_ERROR("server.loading", "[LW Hunt] Failed to spawn final activation object {} for character {} hunt {}.",
            hunt->ActivationGameObjectEntry, r.CharacterGuid, r.HuntId);
        return false;
    }

    r.FinalActivatorGuid = activator->GetGUID();
    ChatHandler(player->GetSession()).PSendSysMessage(
        "|cff00ff00[LW Hunt]|r The signs of {} are unmistakable. Interact with the hunt marker to begin the final confrontation.",
        hunt->Name);
    return true;
}

bool HuntManager::OnFinalActivatorUsed(Player* player, GameObject* gameObject, std::string& message)
{
    if (!player || !gameObject)
    {
        message = "Invalid hunt activation.";
        return false;
    }

    auto it = _runtimes.find(player->GetGUID().GetCounter());
    if (it == _runtimes.end())
    {
        message = "You are not tracking any prey.";
        return false;
    }

    HuntRuntime& r = it->second;
    if (r.State != HuntState::FinalLocated)
    {
        message = "Your hunt is not ready for the final confrontation.";
        return false;
    }

    if (r.FinalActivatorGuid.IsEmpty() || r.FinalActivatorGuid != gameObject->GetGUID())
    {
        message = "This is not your prey's trail.";
        return false;
    }

    if (!SpawnPrey(player, r, true, message))
        return false;

    gameObject->Delete();
    r.FinalActivatorGuid.Clear();
    return true;
}

bool HuntManager::SpawnPrey(Player* player, HuntRuntime& r, bool finalEncounter, std::string& message)
{
    HuntDefinition const* h=GetDefinition(r.HuntId);
    if(!player||!h){message="Unable to resolve hunt prey.";return false;}

    // Recover automatically from a prey creature that vanished or fell far below
    // the player.  This keeps a bad summon from permanently locking the hunt.
    if(!r.ActivePreyGuid.IsEmpty())
    {
        Creature* active=ObjectAccessor::GetCreature(*player,r.ActivePreyGuid);
        if(!active || !active->IsInWorld() || active->GetMapId()!=player->GetMapId() ||
           active->GetDistance2d(player)>120.0f || std::fabs(active->GetPositionZ()-player->GetPositionZ())>25.0f)
        {
            if(active) active->DespawnOrUnsummon();
            r.ActivePreyGuid.Clear();
            r.ActivePreyFinal=false;
            SaveRuntime(r);
        }
        else
        {
            message="Your prey is already active.";
            return false;
        }
    }

    float angle=frand(0.0f,6.2831853f), dist=frand(8.0f,14.0f);
    float x=player->GetPositionX()+std::cos(angle)*dist;
    float y=player->GetPositionY()+std::sin(angle)*dist;
    float z=player->GetPositionZ();

    if(finalEncounter && r.FinalLocationId)
    {
        HuntFinalLocationDefinition const* finalLocation=nullptr;
        for(auto const& l:_finalLocations)
            if(l.Id==r.FinalLocationId){finalLocation=&l;break;}

        if(!finalLocation){message="The final hunt location could not be resolved.";return false;}
        if(player->GetMapId()!=finalLocation->MapId){message="Travel to the marked prey location before starting the final encounter.";return false;}

        float dx=player->GetPositionX()-finalLocation->X;
        float dy=player->GetPositionY()-finalLocation->Y;
        if((dx*dx+dy*dy)>(120.0f*120.0f)){message="Travel to the marked prey location before starting the final encounter.";return false;}

        // The player is standing at the authored site, so summon near the player
        // instead of trusting a hand-entered Z value for the actual creature.
        // The database coordinates still define the POI and activation site.
        angle=frand(0.0f,6.2831853f);
        dist=frand(7.0f,11.0f);
        x=player->GetPositionX()+std::cos(angle)*dist;
        y=player->GetPositionY()+std::sin(angle)*dist;
    }

    if(Map* map=player->GetMap())
    {
        float groundZ=map->GetHeight(x,y,z+10.0f,true,50.0f);
        if(groundZ>INVALID_HEIGHT)
            z=groundZ+0.5f;
    }

    uint32 const preyEntry = ResolvePreyEntry(*h);
    if (!preyEntry) { message="The prey creature template could not be resolved."; return false; }
    TempSummon* prey=player->SummonCreature(preyEntry,x,y,z,player->GetOrientation(),TEMPSUMMON_TIMED_OR_DEAD_DESPAWN,300000);
    if(!prey){message="The prey could not be spawned.";return false;}
    prey->SetLevel(player->GetLevel());
    prey->UpdateAllStats();

    // SetLevel + UpdateAllStats makes the derived template use the hunter's
    // level.  Also enforce a player-relative health floor so a high-level
    // hunter cannot accidentally one-shot a prey whose visual base was a
    // low-level creature such as Hogger.
    float const healthMultiplier = finalEncounter ? h->FinalHealthMultiplier : h->AmbushHealthMultiplier;
    uint64 const playerScaledHealth = static_cast<uint64>(std::max(1.0f, healthMultiplier) * player->GetMaxHealth());
    uint32 const desiredMaxHealth = static_cast<uint32>(std::min<uint64>(std::numeric_limits<uint32>::max(), std::max<uint64>(prey->GetMaxHealth(), playerScaledHealth)));
    prey->SetMaxHealth(desiredMaxHealth);
    prey->SetFullHealth();

    r.ActivePreyGuid=prey->GetGUID(); r.ActivePreyFinal=finalEncounter;
    if(!finalEncounter){r.AmbushesCompleted++; SaveRuntime(r); ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000[LW Hunt]|r {} has found YOU! Drive it off!",h->Name);}
    else ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[LW Hunt]|r {} emerges for the final confrontation!",h->Name);
    prey->AI()->AttackStart(player);
    message=finalEncounter?"Final prey spawned.":"Ambush spawned."; return true;
}

void HuntManager::LocateFinal(Player* player, HuntRuntime& r)
{
    HuntFinalLocationDefinition const* l=SelectFinalLocation(r); if(!l) return;
    r.TrackingProgress=100; r.FinalLocationId=l->Id; r.State=HuntState::FinalLocated; SaveRuntime(r);
    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[LW Hunt]|r Your tracking is complete. {} has been located at {}.", GetDefinition(r.HuntId)->Name,l->LocationName);

    // 3.3.5a's normal guard-direction marker is SMSG_GOSSIP_POI.  Send the
    // selected authored hunt location directly so Hunts do not need a client
    // patch or a permanent points_of_interest row for every possible runtime.
    WorldPacket poi(SMSG_GOSSIP_POI, 64);
    poi << uint32(6);          // normal POI flags used by world-map/minimap markers
    poi << float(l->X);
    poi << float(l->Y);
    poi << uint32(7);          // red "X"-style POI icon
    poi << uint32(0);          // importance
    std::string const poiName = std::string("Prey: ") + GetDefinition(r.HuntId)->Name;
    poi << poiName;
    player->GetSession()->SendPacket(&poi);

    ChatHandler(player->GetSession()).SendSysMessage("|cff00ff00[LW Hunt]|r The prey location has been marked on your map. Travel there and interact with the hunt marker to begin the final confrontation.");
    EnsureFinalActivator(player, r);
}

bool HuntManager::ForceAmbush(Player* player, std::string& message)
{
    if(!player){message="Player required.";return false;} auto it=_runtimes.find(player->GetGUID().GetCounter()); if(it==_runtimes.end()){message="No active hunt.";return false;} return SpawnPrey(player,it->second,false,message);
}

bool HuntManager::ForceFinal(Player* player, std::string& message)
{
    if(!player){message="Player required.";return false;} auto it=_runtimes.find(player->GetGUID().GetCounter()); if(it==_runtimes.end()){message="No active hunt.";return false;} HuntRuntime& r=it->second;
    if(r.State==HuntState::Tracking) LocateFinal(player,r);
    if(r.State!=HuntState::FinalLocated){message="The hunt is not ready for a final encounter.";return false;}
    if(!SpawnPrey(player,r,true,message)) return false;
    RemoveFinalActivator(player,r);
    return true;
}

void HuntManager::Update(uint32 diff)
{
    if(!_enabled) return; if(_updateTimerMs>diff){_updateTimerMs-=diff;return;} _updateTimerMs=250;
    for(auto& [guid,r]:_runtimes)
    {
        Player* p=ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid)); if(!p) continue;

        if(r.State==HuntState::FinalLocated && r.ActivePreyGuid.IsEmpty())
            EnsureFinalActivator(p,r);

        if(r.ActivePreyGuid.IsEmpty()||r.ActivePreyFinal) continue;
        Creature* prey=ObjectAccessor::GetCreature(*p,r.ActivePreyGuid); if(!prey){r.ActivePreyGuid.Clear();continue;}
        HuntDefinition const* h=GetDefinition(r.HuntId); if(!h) continue;
        if(prey->GetHealthPct()<=h->EscapeHealthPct)
        {
            prey->CombatStop(true); prey->SetFlag(UNIT_FIELD_FLAGS,UNIT_FLAG_NON_ATTACKABLE|UNIT_FLAG_IMMUNE_TO_PC);
            ChatHandler(p->GetSession()).PSendSysMessage("|cffffff00[LW Hunt]|r {} breaks away and disappears. Continue tracking it.",h->Name);
            prey->DespawnOrUnsummon(Milliseconds(1500)); r.ActivePreyGuid.Clear(); SaveRuntime(r);
        }
    }
}

std::string HuntManager::BuildStatus(Player const* player) const
{
    std::ostringstream s; if(!_enabled){s<<"Hunts: disabled";return s.str();} if(!player){s<<"Hunts enabled; minimum level "<<uint32(_minimumLevel)<<", XP multiplier "<<_xpMultiplier;return s.str();}
    HuntRuntime const* r=GetRuntime(player); if(!r){s<<"No active hunt.";return s.str();} HuntDefinition const* h=GetDefinition(r->HuntId);
    s<<"Hunt: "<<(h?h->Name:"unknown")<<" | zone="<<r->ZoneId<<" | tracking="<<uint32(r->TrackingProgress)<<"% | ambushes="<<uint32(r->AmbushesCompleted)<<" | state="<<uint32(static_cast<uint8>(r->State)); return s.str();
}
}
