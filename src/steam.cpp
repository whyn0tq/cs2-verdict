#include "steam.h"
#include <cstdlib>
#include <cstring>

static const uint64 SteamIdBase = 76561197960265728ULL;

bool IsDigits(const std::string& text)
{
    if (text.empty())
        return false;
    for (size_t i = 0; i < text.length(); i++)
    {
        if (text[i] < '0' || text[i] > '9')
            return false;
    }
    return true;
}

bool LooksLikeIp(const std::string& text)
{
    int dots = 0;
    int digits = 0;
    for (size_t i = 0; i < text.length(); i++)
    {
        char c = text[i];
        if (c == '.')
        {
            if (digits == 0)
                return false;
            dots++;
            digits = 0;
        }
        else if (c >= '0' && c <= '9')
        {
            digits++;
            if (digits > 3)
                return false;
        }
        else
        {
            return false;
        }
    }
    return dots == 3 && digits > 0;
}

static bool ParseSteam2(const std::string& text, uint64& steamId)
{
    if (text.compare(0, 6, "STEAM_") != 0)
        return false;

    size_t firstColon = text.find(':');
    if (firstColon == std::string::npos)
        return false;
    size_t secondColon = text.find(':', firstColon + 1);
    if (secondColon == std::string::npos)
        return false;

    std::string lowBit = text.substr(firstColon + 1, secondColon - firstColon - 1);
    std::string accountPart = text.substr(secondColon + 1);
    if (!IsDigits(lowBit) || !IsDigits(accountPart))
        return false;

    uint64 account = strtoull(accountPart.c_str(), nullptr, 10) * 2 + strtoull(lowBit.c_str(), nullptr, 10);
    steamId = SteamIdBase + account;
    return true;
}

static bool ParseSteam3(const std::string& text, uint64& steamId)
{
    if (text.compare(0, 5, "[U:1:") != 0 || text[text.length() - 1] != ']')
        return false;

    std::string accountPart = text.substr(5, text.length() - 6);
    if (!IsDigits(accountPart))
        return false;

    steamId = SteamIdBase + strtoull(accountPart.c_str(), nullptr, 10);
    return true;
}

bool ParseSteamId(const std::string& text, uint64& steamId)
{
    if (IsDigits(text) && text.length() == 17)
    {
        steamId = strtoull(text.c_str(), nullptr, 10);
        return steamId > SteamIdBase;
    }
    if (ParseSteam2(text, steamId))
        return true;
    return ParseSteam3(text, steamId);
}

std::string SteamIdToText(uint64 steamId)
{
    return std::to_string(steamId);
}

std::string SteamIdToAuthTail(uint64 steamId)
{
    uint64 account = steamId - SteamIdBase;
    return std::to_string(account % 2) + ":" + std::to_string(account / 2);
}

std::string SteamIdToSteam2(uint64 steamId)
{
    return "STEAM_1:" + SteamIdToAuthTail(steamId);
}

std::string AuthRegexp(uint64 steamId)
{
    return "'^STEAM_[0-9]:" + SteamIdToAuthTail(steamId) + "$'";
}

std::string IpWithoutPort(const std::string& address)
{
    size_t colon = address.find(':');
    if (colon == std::string::npos)
        return address;
    return address.substr(0, colon);
}

std::string CleanText(const std::string& text, size_t maxBytes)
{
    std::string result;
    size_t i = 0;
    while (i < text.length())
    {
        unsigned char c = (unsigned char)text[i];
        size_t length = 1;
        if (c >= 0xF0)
            length = 4;
        else if (c >= 0xE0)
            length = 3;
        else if (c >= 0xC0)
            length = 2;

        if (i + length > text.length())
            break;

        if (length < 4 && c >= 0x20 && c != 0x7F)
        {
            if (result.length() + length > maxBytes)
                break;
            result += text.substr(i, length);
        }
        i += length;
    }
    return result;
}

std::string CleanName(const std::string& name)
{
    std::string result = CleanText(name, 120);
    if (result.empty())
        result = "unnamed";
    return result;
}
