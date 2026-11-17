#pragma once

#include <string>

typedef unsigned long long uint64;

bool IsDigits(const std::string& text);
bool LooksLikeIp(const std::string& text);
bool ParseSteamId(const std::string& text, uint64& steamId);
std::string SteamIdToText(uint64 steamId);
std::string SteamIdToSteam2(uint64 steamId);
std::string SteamIdToAuthTail(uint64 steamId);
std::string AuthRegexp(uint64 steamId);
std::string IpWithoutPort(const std::string& address);
std::string CleanText(const std::string& text, size_t maxBytes);
std::string CleanName(const std::string& name);
