#include "HuntManager.h"

#include "Chat.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "GameObject.h"
#include "GameObjectScript.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptedGossip.h"

namespace
{
enum HuntGossipAction : uint32
{
    ACTION_REQUEST_HUNT = GOSSIP_ACTION_INFO_DEF + 1,
    ACTION_TURN_IN_HUNT = GOSSIP_ACTION_INFO_DEF + 2,
    ACTION_ABANDON_HUNT = GOSSIP_ACTION_INFO_DEF + 3
};

class LivingWorldHuntmasterScript final : public CreatureScript
{
public:
    LivingWorldHuntmasterScript() : CreatureScript("lw_huntmaster") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!sHuntMgr.IsEnabled() || !sHuntMgr.IsHuntGiver(creature->GetEntry()))
            return false;

        lw::HuntRuntime const* runtime = sHuntMgr.GetRuntime(player);
        if (!runtime)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "I seek dangerous prey.", GOSSIP_SENDER_MAIN, ACTION_REQUEST_HUNT);
        }
        else if (runtime->State == lw::HuntState::ReadyToTurnIn)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "I have slain my quarry.", GOSSIP_SENDER_MAIN, ACTION_TURN_IN_HUNT);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Tell me about my current hunt.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "I wish to abandon this hunt.", GOSSIP_SENDER_MAIN, ACTION_ABANDON_HUNT);
        }

        SendGossipMenuFor(player, 1, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);
        std::string message;
        switch (action)
        {
            case ACTION_REQUEST_HUNT:
                sHuntMgr.RequestHunt(player, creature, message);
                break;
            case ACTION_TURN_IN_HUNT:
                sHuntMgr.TurnInHunt(player, creature, message);
                break;
            case ACTION_ABANDON_HUNT:
                sHuntMgr.AbandonHunt(player, message);
                break;
            default:
                message = sHuntMgr.BuildStatus(player);
                break;
        }

        ChatHandler(player->GetSession()).PSendSysMessage("|cff33ccff[LW Hunt]|r {}", message);
        CloseGossipMenuFor(player);
        return true;
    }
};


class LivingWorldHuntActivationScript final : public GameObjectScript
{
public:
    LivingWorldHuntActivationScript() : GameObjectScript("lw_hunt_activation") { }

    bool OnGossipHello(Player* player, GameObject* gameObject) override
    {
        std::string message;
        bool const handled = sHuntMgr.OnFinalActivatorUsed(player, gameObject, message);
        if (!message.empty())
            ChatHandler(player->GetSession()).PSendSysMessage("|cff33ccff[LW Hunt]|r {}", message);
        (void)handled;
        return true; // swallow normal GO behavior for the LW activation marker
    }
};

class LivingWorldHuntPlayerScript final : public PlayerScript
{
public:
    LivingWorldHuntPlayerScript() : PlayerScript("LivingWorldHuntPlayerScript", {
        PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_CREATURE_KILLED_BY_PET
    }) { }

    void OnPlayerCreatureKill(Player* killer, Creature* killed) override
    {
        sHuntMgr.OnCreatureKill(killer, killed);
    }

    void OnPlayerCreatureKilledByPet(Player* owner, Creature* killed) override
    {
        sHuntMgr.OnCreatureKill(owner, killed);
    }
};
}

void AddLivingWorldHuntScripts()
{
    new LivingWorldHuntmasterScript();
    new LivingWorldHuntActivationScript();
    new LivingWorldHuntPlayerScript();
}
