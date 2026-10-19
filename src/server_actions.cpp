#include "server_actions.h"
#include "admins.h"
#include "clients.h"
#include "phrases.h"
#include "player_actions.h"
#include "plugin.h"
#include "settings.h"
#include "steam.h"
#include "common.h"

static std::string AdminText(const char* key, int adminSlot)
{
    std::string text = Text(key);
    Replace(text, "{admin}", AdminDisplayName(adminSlot));
    return text;
}

static bool IsSafeName(const std::string& text)
{
    if (text.empty() || text.length() > 128)
        return false;
    for (size_t i = 0; i < text.length(); i++)
    {
        char c = text[i];
        if (!isalnum((unsigned char)c) && c != '_' && c != '-' && c != '.' && c != '/')
            return false;
    }
    return true;
}

std::string MapTitle(const std::string& map)
{
    for (size_t i = 0; i < g_Settings.maps.size(); i++)
    {
        if (g_Settings.maps[i].value == map)
            return g_Settings.maps[i].title;
    }
    return map;
}

static std::string MapCommand(const std::string& map)
{
    if (IsDigits(map))
        return "host_workshop_map " + map;
    if (engine->IsMapValid(map.c_str()))
        return "changelevel " + map;
    return "ds_workshop_changelevel " + map;
}

void ChangeMap(int adminSlot, const std::string& map)
{
    if (!IsSafeName(map))
        return;

    std::string text = AdminText("Action_Map", adminSlot);
    Replace(text, "{map}", MapTitle(map));
    AnnounceAction(adminSlot, text);

    std::string command = MapCommand(map);
    if (g_Settings.mapChangeDelay <= 0)
    {
        engine->ServerCommand(command.c_str());
        return;
    }

    g_pUtils->CreateTimer((float)g_Settings.mapChangeDelay, [command]() {
        engine->ServerCommand(command.c_str());
        return -1.0f;
    });
}

void RestartGame(int adminSlot)
{
    AnnounceAction(adminSlot, AdminText("Action_Restart", adminSlot));
    engine->ServerCommand("mp_restartgame 1");
}

void SwapTeams(int adminSlot)
{
    AnnounceAction(adminSlot, AdminText("Action_SwapTeams", adminSlot));
    engine->ServerCommand("mp_swapteams");
}

void ExecuteConfig(int adminSlot, const std::string& file)
{
    if (!IsSafeName(file))
        return;

    std::string text = AdminText("Action_Exec", adminSlot);
    Replace(text, "{config}", file);
    AnnounceAction(adminSlot, text);

    std::string command = "exec " + file;
    engine->ServerCommand(command.c_str());
}

void ReloadEveryone(int adminSlot)
{
    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (IsRealClient(slot))
            SyncPlayer(slot);
    }
    if (IsValidSlot(adminSlot))
        ChatTo(adminSlot, Text("EveryoneReloaded"));
}

void RunServerCommand(int adminSlot, const std::string& command)
{
    std::string clean;
    for (size_t i = 0; i < command.length(); i++)
    {
        unsigned char c = (unsigned char)command[i];
        if (c >= 0x20 && c != 0x7F)
            clean += command[i];
    }
    if (clean.empty())
        return;

    META_CONPRINTF("[Verdict] %s ran: %s\n", AdminName(adminSlot).c_str(), clean.c_str());
    engine->ServerCommand(clean.c_str());

    std::string text = Text("RconDone");
    Replace(text, "{command}", clean);
    if (IsValidSlot(adminSlot))
        ChatTo(adminSlot, text);
}
