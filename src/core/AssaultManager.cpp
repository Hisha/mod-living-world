#include "AssaultManager.h"

#include "Creature.h"
#include "CreatureAI.h"
#include "Log.h"
#include "LivingWorld.h"
#include "Map.h"
#include "MapMgr.h"
#include "MotionMaster.h"
#include "PathGenerator.h"
#include "Random.h"
#include "RuntimeEntityGroup.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace lw
{
namespace
{
constexpr float DefaultSearchRadius = 40.0f;
constexpr uint32 DefaultReacquireIntervalMs = 2000;
constexpr uint32 MinimumReacquireIntervalMs = 500;

constexpr uint32 AssaultWanderMinimumDelayMs = 4000;
constexpr uint32 AssaultWanderMaximumDelayMs = 10000;
constexpr uint32 AssaultWanderRetryMinimumMs = 1500;
constexpr uint32 AssaultWanderRetryMaximumMs = 3500;
constexpr uint32 AssaultWanderMoveWatchdogMs = 15000;
constexpr float AssaultWanderArrivalTolerance = 2.0f;
constexpr float AssaultCommanderWanderRadius = 5.0f;
constexpr uint32 AssaultWanderPathAttempts = 8;
constexpr float TwoPi = 6.28318530717958647692f;

// World defenders that are dramatically above the invasion force can dominate
// low-level events (for example level-65 flight gryphons in Westfall). Leave
// ordinary level differences alone, but clamp extreme outliers close to the
// attacking creature's level for the duration of the invasion.
constexpr uint8 DefenderNormalizationThresholdLevels = 10;
constexpr uint8 DefenderNormalizationBonusLevels = 2;
constexpr uint32 EnragedGryphonEntry = 9526;

}

AssaultManager& AssaultManager::Instance()
{
    static AssaultManager instance;
    return instance;
}

void AssaultManager::Reset()
{
    RestoreAllOverrides();
    _activeAssaults.clear();
}

TemporaryNpcCombatOverride* AssaultManager::FindOrCreateOverride(uint64 runtimeId, Creature* target)
{
    if (!target)
    {
        return nullptr;
    }

    for (TemporaryNpcCombatOverride& overrideData : _temporaryNpcOverrides)
    {
        if (overrideData.MapId == target->GetMapId() && overrideData.Guid == target->GetGUID())
        {
            overrideData.RuntimeIds.insert(runtimeId);
            return &overrideData;
        }
    }

    TemporaryNpcCombatOverride overrideData;
    overrideData.Guid = target->GetGUID();
    overrideData.MapId = target->GetMapId();
    overrideData.WasImmuneToNpc = target->IsImmuneToNPC();
    overrideData.OriginalFaction = target->GetFaction();
    overrideData.TemporaryFaction = target->GetFaction();
    overrideData.RuntimeIds.insert(runtimeId);

    _temporaryNpcOverrides.push_back(std::move(overrideData));
    return &_temporaryNpcOverrides.back();
}

void AssaultManager::NormalizeWorldDefenderForAssault(
    uint64 runtimeId,
    Creature* attacker,
    Creature* target)
{
    if (!attacker || !target || !target->IsAlive())
    {
        return;
    }

    // Entry 9526 is the level-65 Enraged Gryphon used as flight-master
    // protection. It is intentionally excluded from Living World.
    // This function is a proven interception point for gryphons that enter
    // combat through AzerothCore's normal faction/AI logic, so terminate the
    // engagement here instead of normalizing the gryphon into an invasion
    // participant.
    if (target->GetEntry() == EnragedGryphonEntry)
    {
        ObjectGuid const gryphonGuid = target->GetGUID();
        ObjectGuid const attackerGuid = attacker->GetGUID();

        target->CombatStop(true);
        if (attacker->GetVictim() == target || attacker->IsInCombat())
            attacker->CombatStop(true);

        LOG_INFO("server.loading",
            "[LW Assault] Runtime #{} HARD-EXCLUDED Enraged Gryphon {} from combat with LW creature {}; combat forcibly stopped.",
            runtimeId,
            gryphonGuid.ToString(),
            attackerGuid.ToString());
        return;
    }

    uint8 const attackerLevel = attacker->GetLevel();
    uint8 const targetLevel = target->GetLevel();

    if (targetLevel <= attackerLevel + DefenderNormalizationThresholdLevels)
    {
        return;
    }

    TemporaryNpcCombatOverride* overrideData = FindOrCreateOverride(runtimeId, target);
    if (!overrideData || overrideData->CombatNormalized)
    {
        return;
    }

    uint8 const normalizedLevel = static_cast<uint8>(std::min<uint32>(
        STRONG_MAX_LEVEL,
        static_cast<uint32>(attackerLevel) + DefenderNormalizationBonusLevels));

    if (normalizedLevel >= targetLevel)
    {
        return;
    }

    overrideData->CombatNormalized = true;
    overrideData->OriginalLevel = targetLevel;
    overrideData->OriginalMaxHealth = target->GetMaxHealth();
    overrideData->OriginalBaseMinDamage = target->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
    overrideData->OriginalBaseMaxDamage = target->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);
    overrideData->OriginalOffMinDamage = target->GetWeaponDamageRange(OFF_ATTACK, MINDAMAGE);
    overrideData->OriginalOffMaxDamage = target->GetWeaponDamageRange(OFF_ATTACK, MAXDAMAGE);
    overrideData->OriginalRangedMinDamage = target->GetWeaponDamageRange(RANGED_ATTACK, MINDAMAGE);
    overrideData->OriginalRangedMaxDamage = target->GetWeaponDamageRange(RANGED_ATTACK, MAXDAMAGE);

    float const healthPct = target->GetMaxHealth() != 0
        ? static_cast<float>(target->GetHealth()) / static_cast<float>(target->GetMaxHealth())
        : 1.0f;

    float const scale = static_cast<float>(normalizedLevel) / static_cast<float>(targetLevel);
    uint32 const normalizedMaxHealth = std::max<uint32>(1u,
        static_cast<uint32>(static_cast<float>(overrideData->OriginalMaxHealth) * scale));

    target->SetLevel(normalizedLevel);
    target->SetMaxHealth(normalizedMaxHealth);
    target->SetHealth(std::max<uint32>(1u,
        static_cast<uint32>(static_cast<float>(normalizedMaxHealth) * healthPct)));

    target->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, overrideData->OriginalBaseMinDamage * scale);
    target->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, overrideData->OriginalBaseMaxDamage * scale);
    target->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, overrideData->OriginalOffMinDamage * scale);
    target->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, overrideData->OriginalOffMaxDamage * scale);
    target->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, overrideData->OriginalRangedMinDamage * scale);
    target->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, overrideData->OriginalRangedMaxDamage * scale);
    target->UpdateDamagePhysical(BASE_ATTACK);
    target->UpdateDamagePhysical(OFF_ATTACK);
    target->UpdateDamagePhysical(RANGED_ATTACK);

    LOG_INFO("server.loading",
        "[LW Assault] Runtime #{} temporarily normalized world defender {} GUID {} from level {} to {} "
        "(health {} -> {}, damage scale {:.2f}).",
        runtimeId,
        target->GetEntry(),
        target->GetGUID().ToString(),
        targetLevel,
        normalizedLevel,
        overrideData->OriginalMaxHealth,
        normalizedMaxHealth,
        scale);
}

bool AssaultManager::EnsureWorldDefenderAttackable(
    uint64 runtimeId,
    Creature* attacker,
    Creature* target,
    float searchRadius)
{
    if (!attacker || !target)
    {
        return false;
    }

    // Never create a temporary attackability/faction override for the
    // flight-master Enraged Gryphon. This prevents later assault logic from
    // accidentally turning it back into a valid LW target after combat was
    // suppressed.
    if (target->GetEntry() == EnragedGryphonEntry)
    {
        target->CombatStop(true);
        if (attacker->GetVictim() == target)
            attacker->CombatStop(true);
        return false;
    }

    TemporaryNpcCombatOverride* existingOverride = FindOrCreateOverride(runtimeId, target);
    if (!existingOverride)
    {
        return false;
    }

    NormalizeWorldDefenderForAssault(runtimeId, attacker, target);

    if (target->IsImmuneToNPC())
    {
        target->SetImmuneToNPC(false);

        LOG_INFO("server.loading",
            "[LW Assault] Runtime #{} temporarily removed UNIT_FLAG_IMMUNE_TO_NPC from world defender {} GUID {}.",
            runtimeId,
            target->GetEntry(),
            target->GetGUID().ToString());
    }

    // Some world defenders remain neutral to the invasion even after NPC immunity
    // is removed. Rather than hard-code a faction, borrow the faction template
    // of a nearby normal defender that this attacker can already attack.
    //
    // That preserves the area's normal defender allegiance (including player
    // friendliness) while making only this live world-defender instance a proper
    // participant in the invasion.
    if (!attacker->IsValidAttackTarget(target))
    {
        Map* map = target->GetMap();
        Creature* factionDonor = nullptr;
        float bestDonorDistance = searchRadius + 1.0f;

        if (map)
        {
            for (auto const& [spawnId, candidate] : map->GetCreatureBySpawnIdStore())
            {
                (void)spawnId;

                if (!candidate ||
                    candidate == attacker ||
                    candidate == target ||
                    !candidate->IsAlive() ||
                    candidate->GetMap() != map)
                {
                    continue;
                }

                float const distance = target->GetDistance(candidate);
                if (distance > searchRadius || distance >= bestDonorDistance)
                {
                    continue;
                }

                if (!attacker->IsValidAttackTarget(candidate))
                {
                    continue;
                }

                factionDonor = candidate;
                bestDonorDistance = distance;
            }
        }

        if (factionDonor)
        {
            uint32 const donorFaction = factionDonor->GetFaction();

            if (target->GetFaction() != donorFaction)
            {
                target->SetFaction(donorFaction);
                existingOverride->TemporaryFaction = donorFaction;
                existingOverride->FactionChanged = true;

                LOG_INFO("server.loading",
                    "[LW Assault] Runtime #{} temporarily changed world defender {} GUID {} faction {} -> {} "
                    "using nearby hostile defender {} GUID {} as faction donor.",
                    runtimeId,
                    target->GetEntry(),
                    target->GetGUID().ToString(),
                    existingOverride->OriginalFaction,
                    donorFaction,
                    factionDonor->GetEntry(),
                    factionDonor->GetGUID().ToString());
            }
        }
        else
        {
            LOG_WARN("server.loading",
                "[LW Assault] Runtime #{} world defender {} GUID {} is still not a valid attack target after "
                "NPC-immunity removal, and no nearby normal hostile defender was available as a faction donor.",
                runtimeId,
                target->GetEntry(),
                target->GetGUID().ToString());
        }
    }

    bool const valid = attacker->IsValidAttackTarget(target);

    if (!valid)
    {
        LOG_WARN("server.loading",
            "[LW Assault] Runtime #{} world defender {} GUID {} remains invalid for attacker {} after temporary override; "
            "attackerReaction={}, targetReaction={}, targetFaction={}.",
            runtimeId,
            target->GetEntry(),
            target->GetGUID().ToString(),
            attacker->GetEntry(),
            static_cast<uint32>(attacker->GetReactionTo(target)),
            static_cast<uint32>(target->GetReactionTo(attacker)),
            target->GetFaction());
    }

    return valid;
}

void AssaultManager::RestoreOverride(TemporaryNpcCombatOverride const& overrideData)
{
    Map* map = sMapMgr->FindMap(overrideData.MapId, 0);
    if (!map)
    {
        return;
    }

    Creature* target = map->GetCreature(overrideData.Guid);
    if (!target)
    {
        return;
    }

    target->CombatStop(true);

    if (overrideData.CombatNormalized)
    {
        float const healthPct = target->GetMaxHealth() != 0
            ? static_cast<float>(target->GetHealth()) / static_cast<float>(target->GetMaxHealth())
            : 1.0f;

        target->SetLevel(overrideData.OriginalLevel);
        target->SetMaxHealth(overrideData.OriginalMaxHealth);
        if (target->IsAlive())
        {
            target->SetHealth(std::max<uint32>(1u,
                static_cast<uint32>(static_cast<float>(overrideData.OriginalMaxHealth) * healthPct)));
        }

        target->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, overrideData.OriginalBaseMinDamage);
        target->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, overrideData.OriginalBaseMaxDamage);
        target->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, overrideData.OriginalOffMinDamage);
        target->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, overrideData.OriginalOffMaxDamage);
        target->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, overrideData.OriginalRangedMinDamage);
        target->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, overrideData.OriginalRangedMaxDamage);
        target->UpdateDamagePhysical(BASE_ATTACK);
        target->UpdateDamagePhysical(OFF_ATTACK);
        target->UpdateDamagePhysical(RANGED_ATTACK);
    }

    if (overrideData.FactionChanged && target->GetFaction() != overrideData.OriginalFaction)
    {
        target->SetFaction(overrideData.OriginalFaction);
    }

    target->SetImmuneToNPC(overrideData.WasImmuneToNpc);

    LOG_INFO("server.loading",
        "[LW Assault] Restored world defender {} GUID {} to faction {}, NPC immunity state {}, level {}{}.",
        target->GetEntry(),
        target->GetGUID().ToString(),
        overrideData.OriginalFaction,
        overrideData.WasImmuneToNpc,
        target->GetLevel(),
        overrideData.CombatNormalized ? " (combat normalization removed)" : "");
}

void AssaultManager::ReleaseRuntimeOverrides(uint64 runtimeId)
{
    for (auto itr = _temporaryNpcOverrides.begin(); itr != _temporaryNpcOverrides.end();)
    {
        itr->RuntimeIds.erase(runtimeId);

        if (!itr->RuntimeIds.empty())
        {
            ++itr;
            continue;
        }

        RestoreOverride(*itr);
        itr = _temporaryNpcOverrides.erase(itr);
    }
}

void AssaultManager::RestoreAllOverrides()
{
    for (TemporaryNpcCombatOverride const& overrideData : _temporaryNpcOverrides)
    {
        RestoreOverride(overrideData);
    }

    _temporaryNpcOverrides.clear();
}

bool AssaultManager::TryStartIdleWander(
    ActiveAssault& assault,
    RuntimeEntity const& entity,
    Creature* creature,
    AssaultWanderState& state)
{
    if (!creature || !creature->IsAlive() || creature->IsInCombat() || creature->GetVictim())
        return false;

    Map* map = creature->GetMap();
    if (!map || map->GetId() != assault.CenterMapId)
        return false;

    bool const isCommander =
        static_cast<TacticalRole>(entity.TacticalRole) == TacticalRole::Commander;

    float const maxRadius = isCommander
        ? std::min<float>(AssaultCommanderWanderRadius, assault.SearchRadius)
        : assault.SearchRadius;

    if (maxRadius <= 1.0f)
        return false;

    for (uint32 attempt = 0; attempt < AssaultWanderPathAttempts; ++attempt)
    {
        float const angle = frand(0.0f, TwoPi);
        float const radiusRoll = std::sqrt(frand(0.0f, 1.0f));
        float const distance = std::max<float>(1.5f, radiusRoll * maxRadius);

        float const requestedX = assault.CenterX + std::cos(angle) * distance;
        float const requestedY = assault.CenterY + std::sin(angle) * distance;
        float requestedZ = map->GetHeight(
            requestedX,
            requestedY,
            assault.CenterZ + 5.0f,
            true,
            50.0f);

        if (requestedZ <= INVALID_HEIGHT)
            continue;

        PathGenerator path(creature);
        bool const pathFound = path.CalculatePath(requestedX, requestedY, requestedZ, false);
        if (!pathFound || (path.GetPathType() & PATHFIND_NOPATH))
            continue;

        Movement::PointsArray pathPoints = path.GetPath();
        if (pathPoints.size() < 2)
            continue;

        G3D::Vector3 const& actualEnd = pathPoints.back();
        float const dx = actualEnd.x - assault.CenterX;
        float const dy = actualEnd.y - assault.CenterY;
        float const centerDistance = std::sqrt(dx * dx + dy * dy);
        if (centerDistance > maxRadius + 2.0f)
            continue;

        // Assault-idle movement is intentionally walking pace. Combat/chase
        // movement remains untouched and can still run normally.
        creature->GetMotionMaster()->MoveSplinePath(&pathPoints, FORCED_MOVEMENT_WALK);

        state.MoveActive = true;
        state.MoveElapsedMs = 0;
        state.DestinationX = actualEnd.x;
        state.DestinationY = actualEnd.y;
        state.DestinationZ = actualEnd.z;
        state.TimerMs = 0;

        LOG_DEBUG("server.loading",
            "[LW Assault] Runtime #{} group #{} creature {} member {} began independent idle wander "
            "to ({:.2f}, {:.2f}, {:.2f}) {:.1f} yd from assault center{}.",
            assault.RuntimeId,
            assault.RuntimeGroupId,
            entity.Entry,
            entity.MemberId,
            state.DestinationX,
            state.DestinationY,
            state.DestinationZ,
            centerDistance,
            isCommander ? " [COMMANDER]" : "");
        return true;
    }

    return false;
}

void AssaultManager::UpdateIdleWandering(ActiveAssault& assault, uint32 diff)
{
    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(assault.RuntimeGroupId);
    if (!group)
        return;

    auto findState = [&assault](ObjectGuid guid) -> AssaultWanderState*
    {
        for (AssaultWanderState& state : assault.WanderStates)
            if (state.Guid == guid)
                return &state;
        return nullptr;
    };

    for (RuntimeEntity const& entity : group->Entities)
    {
        if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
            continue;

        Map* map = sMapMgr->FindMap(entity.MapId, 0);
        if (!map)
            continue;

        Creature* creature = map->GetCreature(entity.Guid);
        if (!creature || !creature->IsAlive())
            continue;

        AssaultWanderState* state = findState(entity.Guid);
        if (!state)
        {
            AssaultWanderState newState;
            newState.Guid = entity.Guid;
            newState.TimerMs = urand(AssaultWanderMinimumDelayMs, AssaultWanderMaximumDelayMs);
            assault.WanderStates.push_back(newState);
            state = &assault.WanderStates.back();
        }

        if (creature->IsInCombat() || creature->GetVictim())
        {
            state->MoveActive = false;
            state->MoveElapsedMs = 0;
            state->TimerMs = urand(AssaultWanderMinimumDelayMs, AssaultWanderMaximumDelayMs);
            continue;
        }

        if (state->MoveActive)
        {
            state->MoveElapsedMs += diff;
            float const destinationDistance = creature->GetDistance(
                state->DestinationX,
                state->DestinationY,
                state->DestinationZ);

            if (destinationDistance <= AssaultWanderArrivalTolerance ||
                state->MoveElapsedMs >= AssaultWanderMoveWatchdogMs)
            {
                state->MoveActive = false;
                state->MoveElapsedMs = 0;
                state->TimerMs = urand(AssaultWanderMinimumDelayMs, AssaultWanderMaximumDelayMs);
            }
            continue;
        }

        if (state->TimerMs > diff)
        {
            state->TimerMs -= diff;
            continue;
        }

        if (!TryStartIdleWander(assault, entity, creature, *state))
            state->TimerMs = urand(AssaultWanderRetryMinimumMs, AssaultWanderRetryMaximumMs);
    }
}

bool AssaultManager::Start(
    uint64 runtimeId,
    uint32 spawnGroupId,
    uint32 radiusYards,
    uint32 reacquireIntervalMs,
    uint32 targetPolicy,
    uint32 assaultCenterRouteNodeId)
{
    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.FindLatestGroup(runtimeId, spawnGroupId);
    if (!group)
    {
        LOG_ERROR("server.loading",
            "[LW Assault] Runtime #{} cannot start assault because spawn group {} has no runtime entity group.",
            runtimeId,
            spawnGroupId);
        return false;
    }

    ActiveAssault assault;
    assault.RuntimeId = runtimeId;
    assault.RuntimeGroupId = group->Id;
    assault.SpawnGroupId = spawnGroupId;
    assault.SearchRadius = radiusYards == 0
        ? DefaultSearchRadius
        : static_cast<float>(radiusYards);
    assault.ReacquireIntervalMs = reacquireIntervalMs == 0
        ? DefaultReacquireIntervalMs
        : std::max<uint32>(MinimumReacquireIntervalMs, reacquireIntervalMs);
    assault.ReacquireTimerMs = 0;
    assault.TargetPolicy = targetPolicy;

    // Assault wandering uses an EXPLICIT authored route-node center.  Do not
    // infer this from current/home/formation positions: those are runtime state
    // and can legitimately be somewhere else when the assault stage begins.
    if (assaultCenterRouteNodeId == 0)
    {
        LOG_ERROR("server.loading",
            "[LW Assault] Runtime #{} cannot start assault for spawn group {} because no assault center route node was authored.",
            runtimeId,
            spawnGroupId);
        return false;
    }

    RouteNodeDefinition const* assaultCenter = sLivingWorldDataMgr.GetRouteNode(assaultCenterRouteNodeId);
    if (!assaultCenter || !assaultCenter->Enabled)
    {
        LOG_ERROR("server.loading",
            "[LW Assault] Runtime #{} cannot start assault for spawn group {} because route node {} is missing or disabled.",
            runtimeId,
            spawnGroupId,
            assaultCenterRouteNodeId);
        return false;
    }

    assault.CenterMapId = assaultCenter->MapId;
    assault.CenterX = assaultCenter->X;
    assault.CenterY = assaultCenter->Y;
    assault.CenterZ = assaultCenter->Z;

    for (RuntimeEntity const& entity : group->Entities)
    {
        if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
            continue;

        Map* map = sMapMgr->FindMap(entity.MapId, 0);
        if (!map)
            continue;

        Creature* creature = map->GetCreature(entity.Guid);
        if (!creature || !creature->IsAlive())
            continue;

        AssaultWanderState wanderState;
        wanderState.Guid = entity.Guid;
        wanderState.TimerMs = urand(
            AssaultWanderMinimumDelayMs,
            AssaultWanderMaximumDelayMs);
        assault.WanderStates.push_back(wanderState);
    }

    _activeAssaults[group->Id] = assault;

    LOG_INFO("server.loading",
        "[LW Assault] Runtime #{} runtime entity group #{} started assault behavior "
        "with {:.1f} yard search radius, {} ms reacquire interval, target policy {}; "
        "idle wandering centered on authored route node {} at ({:.2f}, {:.2f}, {:.2f}), "
        "commanders capped at {:.1f} yd; idle movement uses WALK speed.",
        runtimeId,
        group->Id,
        assault.SearchRadius,
        assault.ReacquireIntervalMs,
        assault.TargetPolicy,
        assaultCenterRouteNodeId,
        assault.CenterX,
        assault.CenterY,
        assault.CenterZ,
        std::min<float>(AssaultCommanderWanderRadius, assault.SearchRadius));

    TryAcquireTargets(_activeAssaults[group->Id]);
    return true;
}

void AssaultManager::Update(uint32 diff)
{
    std::vector<uint64> finished;

    for (auto& [runtimeGroupId, assault] : _activeAssaults)
    {
        RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(runtimeGroupId);
        if (!group || group->State != RuntimeEntityGroupState::Active)
        {
            finished.push_back(runtimeGroupId);
            continue;
        }

        // Enraged Gryphons (entry 9526) around flight masters are level-65
        // world defenders. They can enter combat through normal AzerothCore
        // faction aggro before the assault reacquire pass selects them, which
        // previously left them at full level long enough to erase low-level
        // invasion forces. Normalize an already-active gryphon engagement on
        // every AssaultManager update instead of waiting for the 2-second
        // reacquire timer. The normal temporary-override cleanup still restores
        // the gryphon when the invasion runtime ends.
        for (RuntimeEntity const& entity : group->Entities)
        {
            if (entity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
                continue;

            Map* map = sMapMgr->FindMap(entity.MapId, 0);
            if (!map)
                continue;

            Creature* invasionCreature = map->GetCreature(entity.Guid);
            if (!invasionCreature || !invasionCreature->IsAlive())
                continue;

            Unit* victim = invasionCreature->GetVictim();
            Creature* defender = victim ? victim->ToCreature() : nullptr;
            if (defender && defender->GetEntry() == EnragedGryphonEntry)
            {
                // This is the same proven path that previously logged
                // "temporarily normalized world defender 9526".  Do not let
                // the fight continue long enough to normalize it: immediately
                // terminate both sides of this specific engagement.
                ObjectGuid const gryphonGuid = defender->GetGUID();
                ObjectGuid const invasionGuid = invasionCreature->GetGUID();

                defender->CombatStop(true);
                invasionCreature->CombatStop(true);

                LOG_INFO("server.loading",
                    "[LW Assault] Runtime #{} HARD-EXCLUDED Enraged Gryphon {} after it engaged LW creature {}; combat forcibly stopped in AssaultManager.",
                    assault.RuntimeId,
                    gryphonGuid.ToString(),
                    invasionGuid.ToString());
                continue;
            }
        }

        UpdateIdleWandering(assault, diff);

        if (assault.ReacquireTimerMs > diff)
        {
            assault.ReacquireTimerMs -= diff;
            continue;
        }

        assault.ReacquireTimerMs = assault.ReacquireIntervalMs;
        TryAcquireTargets(assault);
    }

    for (uint64 runtimeGroupId : finished)
    {
        _activeAssaults.erase(runtimeGroupId);
    }
}

bool AssaultManager::TryAcquireTargets(ActiveAssault& assault)
{
    RuntimeEntityGroup* group = sRuntimeEntityGroupMgr.GetGroup(assault.RuntimeGroupId);
    if (!group)
    {
        return false;
    }

    uint32 living = 0;
    uint32 engaged = 0;
    uint32 newlyEngaged = 0;

    // Track how many LW attackers in this runtime are already assigned to each
    // creature. Target selection prefers the least-contested target first, then
    // the nearest target. This prevents the entire invasion from dog-piling a
    // single quest giver while other valid defenders stand untouched.
    std::vector<std::pair<ObjectGuid, uint32>> targetAssignments;

    auto getAssignmentCount = [&targetAssignments](ObjectGuid guid) -> uint32
    {
        for (auto const& [assignedGuid, count] : targetAssignments)
        {
            if (assignedGuid == guid)
            {
                return count;
            }
        }

        return 0;
    };

    auto incrementAssignment = [&targetAssignments](ObjectGuid guid)
    {
        for (auto& [assignedGuid, count] : targetAssignments)
        {
            if (assignedGuid == guid)
            {
                ++count;
                return;
            }
        }

        targetAssignments.emplace_back(guid, 1u);
    };

    // Count existing victims across every active assault group belonging to this
    // runtime so separate LW spawn groups distribute themselves together.
    for (auto const& [otherRuntimeGroupId, otherAssault] : _activeAssaults)
    {
        if (otherAssault.RuntimeId != assault.RuntimeId)
        {
            continue;
        }

        RuntimeEntityGroup* otherGroup = sRuntimeEntityGroupMgr.GetGroup(otherRuntimeGroupId);
        if (!otherGroup)
        {
            continue;
        }

        for (RuntimeEntity const& otherEntity : otherGroup->Entities)
        {
            if (otherEntity.EntityType != static_cast<uint8>(EntityProviderType::Creature))
            {
                continue;
            }

            Map* otherMap = sMapMgr->FindMap(otherEntity.MapId, 0);
            if (!otherMap)
            {
                continue;
            }

            Creature* otherCreature = otherMap->GetCreature(otherEntity.Guid);
            if (!otherCreature || !otherCreature->IsAlive())
            {
                continue;
            }

            if (Unit* victim = otherCreature->GetVictim())
            {
                incrementAssignment(victim->GetGUID());
            }
        }
    }

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

        ++living;

        if (creature->IsInCombat() && creature->GetVictim())
        {
            ++engaged;
            continue;
        }

        Creature* target = nullptr;
        float bestDistance = assault.SearchRadius + 1.0f;
        uint32 bestAssignmentCount = std::numeric_limits<uint32>::max();
        bool targetIsWorldDefender = false;

        // Keep the core's normal hostile acquisition path for defenders that
        // already participate in AzerothCore faction combat.
        if (Unit* hostile = creature->SelectNearestTarget(assault.SearchRadius))
        {
            target = hostile->ToCreature();
            if (target && target->GetEntry() == EnragedGryphonEntry)
            {
                // Entry 9526 is a flight-master defender and is deliberately
                // excluded from LW combat.  Ignore the core hostile result and
                // continue with the normal LW defender scan.
                target = nullptr;
            }

            if (target)
            {
                bestDistance = creature->GetDistance(target);
                bestAssignmentCount = getAssignmentCount(target->GetGUID());
            }
        }

        // Some world defenders never enter AzerothCore's normal hostile-target
        // path. This includes protected service NPCs and ordinary civilian
        // humanoids such as workers/lumberjacks. Scan the map's normal spawned-
        // creature store explicitly so the invasion can pull those defenders
        // into combat without changing their persistent creature templates.
        for (auto const& [spawnId, candidate] : map->GetCreatureBySpawnIdStore())
        {
            (void)spawnId;

            if (!candidate || candidate == creature || !candidate->IsAlive())
            {
                continue;
            }

            // Flight-master Enraged Gryphons are intentionally not participants
            // in Living World.  Do not let the assault target scanner
            // select them even though their normal faction makes them hostile.
            if (candidate->GetEntry() == EnragedGryphonEntry)
            {
                continue;
            }

            if (candidate->GetMap() != map)
            {
                continue;
            }

            float const distance = creature->GetDistance(candidate);
            if (distance > assault.SearchRadius)
            {
                continue;
            }

            CreatureTemplate const* candidateTemplate = candidate->GetCreatureTemplate();
            if (!candidateTemplate)
            {
                continue;
            }

            uint32 const npcFlags = candidateTemplate->npcflag;

            // parameter3 is an assault target-policy bitmask:
            //   bit 0 (1) = quest givers
            //   bit 1 (2) = vendors
            //
            // Flight masters are intentionally NEVER valid LW defenders.
            // Their normal world AI can invoke protected-flight-master mechanics
            // (including high-level defenders), so LW treats them as infrastructure
            // rather than combatants regardless of the legacy bit-2 policy value.
            //
            // Ordinary humanoid civilians do not need a policy bit. They are
            // eligible when they are not friendly to the invader. This catches
            // workers/lumberjacks and similar town NPCs that have npcflag = 0,
            // while avoiding creatures that are already allied with the invasion.
            bool const isQuestGiver = (npcFlags & UNIT_NPC_FLAG_QUESTGIVER) != 0;
            bool const isVendor = (npcFlags & UNIT_NPC_FLAG_VENDOR) != 0;
            bool const isFlightMaster = (npcFlags & UNIT_NPC_FLAG_FLIGHTMASTER) != 0;

            // Hard design rule: flight masters never participate in LW combat.
            // Do this before any attackability override or defender normalization.
            if (isFlightMaster)
            {
                continue;
            }

            bool const isProtectedServiceNpc = isQuestGiver || isVendor;

            bool const allowedQuestGiver =
                (assault.TargetPolicy & 1u) != 0 && isQuestGiver;
            bool const allowedVendor =
                (assault.TargetPolicy & 2u) != 0 && isVendor;

            bool const allowedCivilianHumanoid =
                !isProtectedServiceNpc &&
                candidateTemplate->type == CREATURE_TYPE_HUMANOID &&
                !candidate->IsFriendlyTo(creature);

            if (!allowedQuestGiver &&
                !allowedVendor &&
                !allowedCivilianHumanoid)
            {
                continue;
            }

            if (candidate->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) ||
                candidate->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            {
                continue;
            }

            uint32 const assignmentCount = getAssignmentCount(candidate->GetGUID());

            // Prefer the target with fewer LW attackers already assigned.
            // Distance breaks ties so the assault still looks geographically
            // natural instead of sending creatures across town unnecessarily.
            if (target)
            {
                if (assignmentCount > bestAssignmentCount)
                {
                    continue;
                }

                if (assignmentCount == bestAssignmentCount && distance >= bestDistance)
                {
                    continue;
                }
            }

            target = candidate;
            bestDistance = distance;
            bestAssignmentCount = assignmentCount;
            targetIsWorldDefender = true;
        }

        if (!target)
        {
            continue;
        }

        // Targets can come from AzerothCore's hostile-target search or LW's
        // explicit world-defender scan. Apply the exclusion here too so no
        // flight master can reach normalization, temporary overrides, or AttackStart().
        if (CreatureTemplate const* targetTemplate = target->GetCreatureTemplate())
        {
            if ((targetTemplate->npcflag & UNIT_NPC_FLAG_FLIGHTMASTER) != 0)
            {
                LOG_DEBUG("server.loading",
                    "[LW Assault] Runtime #{} ignored flight master {} GUID {} as an invasion defender.",
                    assault.RuntimeId,
                    target->GetEntry(),
                    target->GetGUID().ToString());
                continue;
            }
        }

        if (creature->AI())
        {
            // Normal faction-hostile defenders can also be extreme level outliers
            // (notably flight-master gryphons), so normalization applies whether
            // the target came from the core hostile path or our explicit scan.
            NormalizeWorldDefenderForAssault(assault.RuntimeId, creature, target);

            if (targetIsWorldDefender)
            {
                if (!EnsureWorldDefenderAttackable(
                        assault.RuntimeId,
                        creature,
                        target,
                        assault.SearchRadius))
                {
                    LOG_WARN("server.loading",
                        "[LW Assault] Runtime #{} creature {} member {} could not make world defender {} GUID {} attackable by NPCs.",
                        assault.RuntimeId,
                        entity.Entry,
                        entity.MemberId,
                        target->GetEntry(),
                        target->GetGUID().ToString());
                    continue;
                }
}

            creature->AI()->AttackStart(target);

            if (targetIsWorldDefender)
            {
                if (target->AI() && target->CanStartAttack(creature, true))
                {
                    target->AI()->AttackStart(creature);
                }
}

            bool const hasCorrectVictim = creature->GetVictim() == target;

            if (hasCorrectVictim)
            {
                ++engaged;
                ++newlyEngaged;
                incrementAssignment(target->GetGUID());

                LOG_DEBUG("server.loading",
                    "[LW Assault] Runtime #{} creature {} member {} engaged target {} GUID {}{}.",
                    assault.RuntimeId,
                    entity.Entry,
                    entity.MemberId,
                    target->GetEntry(),
                    target->GetGUID().ToString(),
                    targetIsWorldDefender ? " via explicit world-defender assault policy" : "");
            }
            else
            {
                LOG_DEBUG("server.loading",
                    "[LW Assault] Runtime #{} creature {} member {} failed to establish target {} GUID {}{}; "
                    "current victim={}, inCombat={}.",
                    assault.RuntimeId,
                    entity.Entry,
                    entity.MemberId,
                    target->GetEntry(),
                    target->GetGUID().ToString(),
                    targetIsWorldDefender ? " via explicit world-defender assault policy" : "",
                    creature->GetVictim() ? creature->GetVictim()->GetEntry() : 0,
                    creature->IsInCombat());
            }
        }
    }

    if (newlyEngaged != 0)
    {
        LOG_INFO("server.loading",
            "[LW Assault] Runtime #{} runtime entity group #{} acquired {} new target(s); "
            "{}/{} living invader(s) are engaged.",
            assault.RuntimeId,
            assault.RuntimeGroupId,
            newlyEngaged,
            engaged,
            living);
    }

    return engaged != 0;
}

void AssaultManager::CancelRuntime(uint64 runtimeId)
{
    uint32 removed = 0;

    for (auto itr = _activeAssaults.begin(); itr != _activeAssaults.end();)
    {
        if (itr->second.RuntimeId == runtimeId)
        {
            itr = _activeAssaults.erase(itr);
            ++removed;
        }
        else
        {
            ++itr;
        }
    }

    ReleaseRuntimeOverrides(runtimeId);

    if (removed != 0)
    {
        LOG_INFO("server.loading",
            "[LW Assault] Cancelled {} active assault behavior(s) for runtime #{}.",
            removed,
            runtimeId);
    }
}
}
