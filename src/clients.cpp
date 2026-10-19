#include "clients.h"
#include "settings.h"
#include "steam.h"
#include "common.h"

ClientInfo g_Clients[MAX_SLOTS];
std::vector<RecentClient> g_RecentClients;

void RememberClient(int slot, uint64 steamId)
{
    if (!IsValidSlot(slot))
        return;

    ClientInfo& client = g_Clients[slot];
    client.active = true;
    client.steamId = steamId;
    client.name = CleanName(g_pPlayers->GetPlayerName(slot) ? g_pPlayers->GetPlayerName(slot) : "");

    const char* address = g_pPlayers->GetIpAddress(slot);
    client.ip = address ? IpWithoutPort(address) : "";

    RemoveRecentClient(steamId);
}

void ForgetClient(int slot)
{
    if (!IsValidSlot(slot))
        return;

    ClientInfo& client = g_Clients[slot];
    if (client.active && client.steamId != 0)
    {
        RecentClient recent;
        recent.steamId = client.steamId;
        recent.name = client.name;
        recent.ip = client.ip;
        for (int i = 0; i < 4; i++)
            recent.punished[i] = false;

        g_RecentClients.push_back(recent);
        while ((int)g_RecentClients.size() > g_Settings.offlinePunishCount && !g_RecentClients.empty())
            g_RecentClients.erase(g_RecentClients.begin());
    }

    client.active = false;
    client.steamId = 0;
    client.name.clear();
    client.ip.clear();
}

bool IsSameClient(int slot, uint64 steamId)
{
    if (!IsValidSlot(slot))
        return false;
    return g_Clients[slot].active && g_Clients[slot].steamId == steamId;
}

bool IsRealClient(int slot)
{
    if (!IsValidSlot(slot))
        return false;
    return g_Clients[slot].active && g_Clients[slot].steamId != 0;
}

uint64 ClientSteamId(int slot)
{
    if (!IsValidSlot(slot))
        return 0;
    return g_Clients[slot].steamId;
}

std::string ClientName(int slot)
{
    if (slot == CONSOLE_SLOT)
        return "Console";
    if (!IsValidSlot(slot))
        return "";

    const char* current = g_pPlayers->GetPlayerName(slot);
    if (current && current[0])
    {
        std::string name = CleanName(current);
        if (g_Clients[slot].active)
            g_Clients[slot].name = name;
        return name;
    }
    return g_Clients[slot].name;
}

std::string ClientIp(int slot)
{
    if (!IsValidSlot(slot))
        return "";
    return g_Clients[slot].ip;
}

int FindClientBySteamId(uint64 steamId)
{
    if (steamId == 0)
        return -1;
    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (g_Clients[slot].active && g_Clients[slot].steamId == steamId)
            return slot;
    }
    return -1;
}

RecentClient* FindRecentClient(uint64 steamId)
{
    for (size_t i = 0; i < g_RecentClients.size(); i++)
    {
        if (g_RecentClients[i].steamId == steamId)
            return &g_RecentClients[i];
    }
    return nullptr;
}

void RemoveRecentClient(uint64 steamId)
{
    for (size_t i = 0; i < g_RecentClients.size(); i++)
    {
        if (g_RecentClients[i].steamId == steamId)
        {
            g_RecentClients.erase(g_RecentClients.begin() + i);
            return;
        }
    }
}
