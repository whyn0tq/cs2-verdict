#pragma once

#include <charconv>
#include <string>
#include <string_view>
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

inline std::string ClientKey(int slot)
{
    return std::to_string(slot) + ":" + std::to_string(ClientSteamId(slot));
}

inline int ClientFromKey(std::string_view key)
{
    size_t colon = key.find(':');
    if (colon == std::string_view::npos)
        return -1;

    int slot = -1;
    uint64 steamId = 0;
    const char* separator = key.data() + colon;
    const char* end = key.data() + key.size();
    auto slotResult = std::from_chars(key.data(), separator, slot);
    auto idResult = std::from_chars(separator + 1, end, steamId);
    if (slotResult.ec != std::errc{} || slotResult.ptr != separator ||
        idResult.ec != std::errc{} || idResult.ptr != end || steamId == 0)
        return -1;
    return IsSameClient(slot, steamId) ? slot : -1;
}
