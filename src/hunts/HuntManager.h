#ifndef MOD_LIVING_WORLD_HUNT_MANAGER_H
#define MOD_LIVING_WORLD_HUNT_MANAGER_H

#include "Define.h"
#include "ObjectGuid.h"

#include <string>
#include <unordered_map>
#include <vector>

class Creature;
class GameObject;
class Player;

namespace lw
{
enum class HuntState : uint8
{
    None = 0,
    Tracking = 1,
    FinalLocated = 2,
    ReadyToTurnIn = 3
};

struct HuntDefinition
{
    uint32 Id = 0;
    std::string Name;
    uint8 MinLevel = 10;
    uint8 MaxLevel = 80;
    uint32 PreyCreatureEntry = 0;
    uint32 PreyLwTemplateId = 0;
    uint32 ActivationGameObjectEntry = 0;
    float AmbushHealthMultiplier = 4.0f;
    float FinalHealthMultiplier = 6.0f;
    uint8 EscapeHealthPct = 50;
    uint8 AmbushCount = 2;
    bool Enabled = false;
};

struct HuntZoneDefinition
{
    uint32 Id = 0;
    uint32 ZoneId = 0;
    uint16 MapId = 0;
    std::string Name;
    uint8 MinLevel = 1;
    uint8 MaxLevel = 80;
    uint32 Weight = 100;
    bool Enabled = false;
};

struct HuntFinalLocationDefinition
{
    uint32 Id = 0;
    uint32 ZoneId = 0;
    uint16 MapId = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Orientation = 0.0f;
    std::string LocationName;
    uint32 Weight = 100;
    bool Enabled = false;
};


struct HuntGiverDefinition
{
    uint32 Id = 0;
    uint32 CreatureEntry = 0;
    std::string CityName;
    uint16 MapId = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    bool Enabled = false;
};

struct HuntRuntime
{
    uint32 CharacterGuid = 0;
    uint32 HuntId = 0;
    uint32 GiverEntry = 0;
    uint32 GiverSpawnId = 0;
    uint32 ZoneId = 0;
    uint32 FinalLocationId = 0;
    uint8 TrackingProgress = 0;
    uint8 AmbushesCompleted = 0;
    HuntState State = HuntState::None;
    ObjectGuid ActivePreyGuid;
    bool ActivePreyFinal = false;
    ObjectGuid FinalActivatorGuid;
};

class HuntManager
{
public:
    static HuntManager& Instance();

    void Configure(bool enabled, uint8 minimumLevel, float xpMultiplier, bool debug);
    void Reset();
    void LoadDefinitions();
    void Initialize();
    void Update(uint32 diff);

    bool IsEnabled() const { return _enabled; }
    uint8 GetMinimumLevel() const { return _minimumLevel; }
    float GetXpMultiplier() const { return _xpMultiplier; }

    bool HasActiveHunt(Player const* player) const;
    HuntRuntime const* GetRuntime(Player const* player) const;
    HuntDefinition const* GetDefinition(uint32 huntId) const;

    bool RequestHunt(Player* player, Creature* giver, std::string& message);
    bool AbandonHunt(Player* player, std::string& message);
    bool TurnInHunt(Player* player, Creature* giver, std::string& message);
    void OnCreatureKill(Player* player, Creature* killed);
    bool OnFinalActivatorUsed(Player* player, GameObject* gameObject, std::string& message);

    bool AddProgress(Player* player, uint8 amount, std::string& message);
    bool ForceAmbush(Player* player, std::string& message);
    bool ForceFinal(Player* player, std::string& message);
    std::string BuildStatus(Player const* player) const;
    std::string BuildStats(Player const* player) const;

    bool IsHuntGiver(uint32 creatureEntry) const;
    bool IsGuardLocator(uint32 creatureEntry) const;
    bool SendHuntmasterLocation(Player* player, uint32 guardEntry, std::string& message) const;

private:
    HuntManager() = default;

    void LoadRuntimes();
    void SaveRuntime(HuntRuntime const& runtime);
    void DeleteRuntime(uint32 characterGuid);
    HuntZoneDefinition const* SelectZone(uint8 playerLevel) const;
    HuntZoneDefinition const* GetZone(uint32 zoneId) const;
    HuntFinalLocationDefinition const* SelectFinalLocation(HuntRuntime const& runtime) const;
    bool SpawnPrey(Player* player, HuntRuntime& runtime, bool finalEncounter, std::string& message);
    uint32 ResolvePreyEntry(HuntDefinition const& hunt) const;
    HuntFinalLocationDefinition const* GetFinalLocation(uint32 finalLocationId) const;
    bool EnsureFinalActivator(Player* player, HuntRuntime& runtime);
    void RemoveFinalActivator(Player* player, HuntRuntime& runtime);
    void LocateFinal(Player* player, HuntRuntime& runtime);
    uint8 GetNextAmbushThreshold(HuntRuntime const& runtime, HuntDefinition const& hunt) const;

    bool _enabled = false;
    bool _debug = false;
    uint8 _minimumLevel = 10;
    float _xpMultiplier = 0.75f;
    uint32 _updateTimerMs = 0;

    std::unordered_map<uint32, HuntDefinition> _hunts;
    std::vector<HuntZoneDefinition> _zones;
    std::vector<HuntFinalLocationDefinition> _finalLocations;
    std::unordered_map<uint32, uint32> _giverEntries;
    std::unordered_map<uint32, HuntGiverDefinition> _givers;
    std::unordered_map<uint32, uint32> _guardLocators;
    std::unordered_map<uint32, HuntRuntime> _runtimes;
};
}

#define sHuntMgr lw::HuntManager::Instance()

#endif
