#include "management_menu.h"
#include "admin_menu.h"
#include "admins.h"
#include "clients.h"
#include "entities.h"
#include "phrases.h"
#include "player_actions.h"
#include "punishments.h"
#include "server_actions.h"
#include "settings.h"
#include "steam.h"
#include "common.h"

enum PlayerAction
{
    ActionKill,
    ActionSlap,
    ActionBurn,
    ActionTimeBomb,
    ActionFireBomb,
    ActionFreezeBomb,
    ActionKick,
    ActionHealth,
    ActionArmor,
    ActionGravity,
    ActionSpeed,
    ActionFreeze,
    ActionGod,
    ActionNoclip,
    ActionTeam,
    ActionRename,
    ActionRespawn,
    ActionWho,
    ActionCount
};

struct PlayerActionInfo
{
    const char* identity;
    const char* title;
    const char* permission;
};

static const PlayerActionInfo g_PlayerActions[ActionCount] = {
    { "kill", "Item_Kill", "@admin/kill" },
    { "slap", "Item_Slap", "@admin/slap" },
    { "burn", "Item_Burn", "@admin/burn" },
    { "timebomb", "Item_TimeBomb", "@admin/timebomb" },
    { "firebomb", "Item_FireBomb", "@admin/firebomb" },
    { "freezebomb", "Item_FreezeBomb", "@admin/freezebomb" },
    { "kick", "Item_Kick", "@admin/kick" },
    { "hp", "Item_Health", "@admin/hp" },
    { "armor", "Item_Armor", "@admin/armor" },
    { "gravity", "Item_Gravity", "@admin/gravity" },
    { "speed", "Item_Speed", "@admin/speed" },
    { "freeze", "Item_Freeze", "@admin/freeze" },
    { "god", "Item_God", "@admin/god" },
    { "noclip", "Item_Noclip", "@admin/noclip" },
    { "changeteam", "Item_ChangeTeam", "@admin/changeteam" },
    { "rename", "Item_Rename", "@admin/rename" },
    { "respawn", "Item_Respawn", "@admin/respawn" },
    { "who", "Item_Who", "@admin/who" }
};

static const int g_SlapValues[] = { 0, 1, 5, 10, 20, 50, 99 };
static const int g_BurnValues[] = { 5, 10, 20, 30, 60 };
static const int g_HealthValues[] = { 1, 10, 50, 100, 200, 500 };
static const int g_ArmorValues[] = { 0, 50, 100 };
static const int g_GravityValues[] = { 10, 25, 50, 100, 200, 400 };
static const int g_SpeedValues[] = { 50, 100, 150, 200, 300 };
static const int g_FreezeValues[] = { 5, 10, 20, 60, 0 };

static bool g_WaitingForName[MAX_SLOTS];
static bool g_WaitingForCommand[MAX_SLOTS];
static int g_RenameTarget[MAX_SLOTS];
static uint64 g_RenameSteamId[MAX_SLOTS];

static void ShowActionTargets(int slot, int action);

void ResetManagementState(int slot)
{
    if (!IsValidSlot(slot))
        return;
    g_WaitingForName[slot] = false;
    g_WaitingForCommand[slot] = false;
    g_RenameTarget[slot] = -1;
    g_RenameSteamId[slot] = 0;
}

static bool IsPlayingTeam(int team)
{
    return team == TEAM_T || team == TEAM_CT;
}

static bool NeedsAlivePlayer(int action)
{
    switch (action)
    {
    case ActionKill:
    case ActionSlap:
    case ActionBurn:
    case ActionTimeBomb:
    case ActionFireBomb:
    case ActionFreezeBomb:
    case ActionHealth:
    case ActionArmor:
    case ActionGravity:
    case ActionSpeed:
    case ActionFreeze:
    case ActionGod:
    case ActionNoclip:
        return true;
    }
    return false;
}

static bool FitsAction(int adminSlot, int targetSlot, int action)
{
    if (!IsRealClient(targetSlot))
        return false;
    if (action == ActionWho)
        return true;
    if (!CanTarget(adminSlot, targetSlot))
        return false;
    if (NeedsAlivePlayer(action))
        return IsAlive(targetSlot);

    switch (action)
    {
    case ActionRespawn:
        return !IsAlive(targetSlot) && IsPlayingTeam(TeamOf(targetSlot));
    case ActionKick:
        return targetSlot != adminSlot;
    }
    return true;
}

static std::string TargetBack(int targetSlot)
{
    return std::to_string(targetSlot) + ":" + SteamIdToText(ClientSteamId(targetSlot));
}

static int TargetFromBack(const std::string& text)
{
    size_t colon = text.find(':');
    if (colon == std::string::npos)
        return -1;

    int slot = atoi(text.substr(0, colon).c_str());
    uint64 steamId = strtoull(text.substr(colon + 1).c_str(), nullptr, 10);
    return IsSameClient(slot, steamId) ? slot : -1;
}

static std::string TargetTitle(int targetSlot)
{
    std::string team = TeamName(TeamOf(targetSlot));
    return g_pMenus->escapeString(ClientName(targetSlot)) + " [" + team + "]";
}

static const int* ValuesFor(int action, int& count)
{
    switch (action)
    {
    case ActionSlap:
        count = sizeof(g_SlapValues) / sizeof(int);
        return g_SlapValues;
    case ActionBurn:
        count = sizeof(g_BurnValues) / sizeof(int);
        return g_BurnValues;
    case ActionHealth:
        count = sizeof(g_HealthValues) / sizeof(int);
        return g_HealthValues;
    case ActionArmor:
        count = sizeof(g_ArmorValues) / sizeof(int);
        return g_ArmorValues;
    case ActionGravity:
        count = sizeof(g_GravityValues) / sizeof(int);
        return g_GravityValues;
    case ActionSpeed:
        count = sizeof(g_SpeedValues) / sizeof(int);
        return g_SpeedValues;
    case ActionFreeze:
        count = sizeof(g_FreezeValues) / sizeof(int);
        return g_FreezeValues;
    }
    count = 0;
    return nullptr;
}

static std::string ValueLabel(int action, int value)
{
    switch (action)
    {
    case ActionSlap:
    case ActionHealth:
        return std::to_string(value) + " HP";
    case ActionBurn:
        return std::to_string(value) + Text("Seconds");
    case ActionFreeze:
        if (value <= 0)
            return Text("Unfreeze");
        return std::to_string(value) + Text("Seconds");
    case ActionGravity:
    case ActionSpeed:
        return FormatMultiplier(value / 100.0f);
    }
    return std::to_string(value);
}

static bool ApplyValue(int slot, int action, int target, int value)
{
    switch (action)
    {
    case ActionSlap:
        return SlapPlayer(target, slot, value);
    case ActionBurn:
        return BurnPlayer(target, slot, value);
    case ActionHealth:
        return SetPlayerHealth(target, slot, value);
    case ActionArmor:
        return SetPlayerArmor(target, slot, value);
    case ActionGravity:
        return SetPlayerGravity(target, slot, value / 100.0f);
    case ActionSpeed:
        return SetPlayerSpeed(target, slot, value / 100.0f);
    case ActionFreeze:
        return FreezePlayer(target, slot, value);
    }
    return false;
}

static void ShowValueMenu(int slot, int action, int targetSlot)
{
    Menu menu;
    std::string title = std::string(Phrase(g_PlayerActions[action].title)) + ": " + ClientName(targetSlot);
    g_pMenus->SetTitleMenu(menu, title.c_str());

    int count = 0;
    const int* values = ValuesFor(action, count);
    for (int i = 0; i < count; i++)
    {
        std::string back = std::to_string(values[i]) + "|" + TargetBack(targetSlot);
        g_pMenus->AddItemMenu(menu, back.c_str(), ValueLabel(action, values[i]).c_str());
    }

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [action](const char* back, const char* front, int item, int slot) {
        if (item == 7)
        {
            ShowActionTargets(slot, action);
            return;
        }
        if (item >= 7 || !back)
            return;

        std::string text = back;
        size_t separator = text.find('|');
        if (separator == std::string::npos)
            return;

        int value = atoi(text.substr(0, separator).c_str());
        int target = TargetFromBack(text.substr(separator + 1));
        bool done = target != -1 && ApplyValue(slot, action, target, value);

        if (done && action == ActionSlap && IsAlive(target))
            ShowValueMenu(slot, action, target);
        else
            ShowActionTargets(slot, action);
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static void ShowTeamChoice(int slot, int targetSlot)
{
    Menu menu;
    std::string title = Text("SelectTeam");
    Replace(title, "{target}", ClientName(targetSlot));
    g_pMenus->SetTitleMenu(menu, title.c_str());

    int teams[] = { TEAM_T, TEAM_CT, TEAM_SPECTATOR };
    for (int i = 0; i < 3; i++)
    {
        std::string back = std::to_string(teams[i]) + "|" + TargetBack(targetSlot);
        int state = TeamOf(targetSlot) == teams[i] ? ITEM_DISABLED : ITEM_DEFAULT;
        g_pMenus->AddItemMenu(menu, back.c_str(), TeamName(teams[i]).c_str(), state);
    }

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [](const char* back, const char* front, int item, int slot) {
        if (item == 7)
        {
            ShowActionTargets(slot, ActionTeam);
            return;
        }
        if (item >= 7 || !back)
            return;

        std::string text = back;
        size_t separator = text.find('|');
        if (separator == std::string::npos)
            return;

        int team = atoi(text.substr(0, separator).c_str());
        int target = TargetFromBack(text.substr(separator + 1));
        if (target != -1)
            MovePlayerToTeam(target, slot, team);
        ShowActionTargets(slot, ActionTeam);
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static void AskForName(int slot, int targetSlot)
{
    g_WaitingForName[slot] = true;
    g_RenameTarget[slot] = targetSlot;
    g_RenameSteamId[slot] = ClientSteamId(targetSlot);
    g_pMenus->ClosePlayerMenu(slot);

    std::string text = Text("EnterNewName");
    Replace(text, "{target}", ClientName(targetSlot));
    ChatTo(slot, text);
}

static void AskForCommand(int slot)
{
    g_WaitingForCommand[slot] = true;
    g_pMenus->ClosePlayerMenu(slot);
    ChatTo(slot, Text("EnterRconCommand"));
}

bool HandleMenuTextInput(int slot, const std::string& text)
{
    if (!IsValidSlot(slot))
        return false;

    if (g_WaitingForCommand[slot])
    {
        g_WaitingForCommand[slot] = false;
        if (HasPermission(slot, "@admin/rcon"))
            RunServerCommand(slot, text);
        return true;
    }

    if (!g_WaitingForName[slot])
        return false;

    g_WaitingForName[slot] = false;
    int target = g_RenameTarget[slot];
    if (!IsSameClient(target, g_RenameSteamId[slot]))
    {
        ChatTo(slot, Text("PlayerLeft"));
        return true;
    }

    RenamePlayer(target, slot, text);
    ShowActionTargets(slot, ActionRename);
    return true;
}

static void RunAction(int slot, int action, int target)
{
    int count = 0;
    if (ValuesFor(action, count))
    {
        ShowValueMenu(slot, action, target);
        return;
    }

    switch (action)
    {
    case ActionKill:
        SlayPlayer(target, slot);
        break;
    case ActionKick:
        KickWithAnnounce(target, slot, "");
        break;
    case ActionTimeBomb:
        ToggleBomb(target, slot, BOMB_TIME);
        break;
    case ActionFireBomb:
        ToggleBomb(target, slot, BOMB_FIRE);
        break;
    case ActionFreezeBomb:
        ToggleBomb(target, slot, BOMB_FREEZE);
        break;
    case ActionGod:
        ToggleGodMode(target, slot);
        break;
    case ActionNoclip:
        ToggleNoclip(target, slot);
        break;
    case ActionTeam:
        ShowTeamChoice(slot, target);
        return;
    case ActionRename:
        AskForName(slot, target);
        return;
    case ActionRespawn:
        RespawnPlayer(target, slot);
        break;
    case ActionWho:
        g_pMenus->ClosePlayerMenu(slot);
        ShowPlayerInfo(slot, true, target);
        return;
    }
    ShowActionTargets(slot, action);
}

static void ShowActionTargets(int slot, int action)
{
    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase(g_PlayerActions[action].title));

    bool found = false;
    for (int target = 0; target < MAX_SLOTS; target++)
    {
        if (!FitsAction(slot, target, action))
            continue;
        g_pMenus->AddItemMenu(menu, TargetBack(target).c_str(), TargetTitle(target).c_str());
        found = true;
    }
    if (!found)
        g_pMenus->AddItemMenu(menu, "", Phrase("NoPlayers"), ITEM_DISABLED);

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [action](const char* back, const char* front, int item, int slot) {
        if (item == 7)
        {
            ShowCategory(slot, "players");
            return;
        }
        if (item >= 7)
            return;

        int target = TargetFromBack(back ? back : "");
        if (target == -1 || !FitsAction(slot, target, action))
        {
            ChatTo(slot, Text("PlayerLeft"));
            ShowActionTargets(slot, action);
            return;
        }
        RunAction(slot, action, target);
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static void ShowOptionsMenu(int slot, const char* titleKey, const std::vector<NamedOption>& options, bool isMaps)
{
    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase(titleKey));

    for (size_t i = 0; i < options.size(); i++)
        g_pMenus->AddItemMenu(menu, options[i].value.c_str(), options[i].title.c_str());
    if (options.empty())
        g_pMenus->AddItemMenu(menu, "", Phrase(isMaps ? "NoMaps" : "NoConfigs"), ITEM_DISABLED);

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [isMaps](const char* back, const char* front, int item, int slot) {
        if (item == 7)
        {
            ShowCategory(slot, "server");
            return;
        }
        if (item >= 7 || !back || !back[0])
            return;

        g_pMenus->ClosePlayerMenu(slot);
        if (isMaps)
            ChangeMap(slot, back);
        else
            ExecuteConfig(slot, back);
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static void RegisterPlayerItems()
{
    RegisterMenuCategory("players", "Category_Players", nullptr);
    for (int action = 0; action < ActionCount; action++)
    {
        const PlayerActionInfo& info = g_PlayerActions[action];
        RegisterMenuItem(info.identity, info.title, "players", info.permission, nullptr,
            [action](int slot, const char*, const char*, const char*) { ShowActionTargets(slot, action); });
    }
}

static void RegisterServerItems()
{
    RegisterMenuCategory("server", "Category_Server", nullptr);

    RegisterMenuItem("maps", "Item_Maps", "server", "@admin/maps", nullptr,
        [](int slot, const char*, const char*, const char*) { ShowOptionsMenu(slot, "SelectMap", g_Settings.maps, true); });

    RegisterMenuItem("restart", "Item_Restart", "server", "@admin/restart", nullptr,
        [](int slot, const char*, const char*, const char*) {
            RestartGame(slot);
            ShowCategory(slot, "server");
        });

    RegisterMenuItem("swapteams", "Item_SwapTeams", "server", "@admin/swapteams", nullptr,
        [](int slot, const char*, const char*, const char*) {
            SwapTeams(slot);
            ShowCategory(slot, "server");
        });

    RegisterMenuItem("exec", "Item_Exec", "server", "@admin/exec", nullptr,
        [](int slot, const char*, const char*, const char*) { ShowOptionsMenu(slot, "SelectConfig", g_Settings.configs, false); });

    RegisterMenuItem("rcon", "Item_Rcon", "server", "@admin/rcon", nullptr,
        [](int slot, const char*, const char*, const char*) { AskForCommand(slot); });

    RegisterMenuItem("reload", "Item_ReloadAdmins", "server", "@admin/reload_admin", nullptr,
        [](int slot, const char*, const char*, const char*) {
            ReloadEveryone(slot);
            ShowCategory(slot, "server");
        });
}

void RegisterManagementMenuItems()
{
    RegisterPlayerItems();
    RegisterServerItems();
}
