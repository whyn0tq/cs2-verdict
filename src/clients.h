#pragma once

#include <string>
#include <vector>

typedef unsigned long long uint64;

struct ClientInfo
{
    bool active;
    uint64 steamId;
    std::string name;
    std::string ip;
};

struct RecentClient
{
    uint64 steamId;
    std::string name;
    std::string ip;
    bool punished[4];
};

extern ClientInfo g_Clients[64];
extern std::vector<RecentClient> g_RecentClients;

void RememberClient(int slot, uint64 steamId);
void ForgetClient(int slot);
bool IsSameClient(int slot, uint64 steamId);
bool IsRealClient(int slot);
uint64 ClientSteamId(int slot);
std::string ClientName(int slot);
std::string ClientIp(int slot);
int FindClientBySteamId(uint64 steamId);
RecentClient* FindRecentClient(uint64 steamId);
void RemoveRecentClient(uint64 steamId);
