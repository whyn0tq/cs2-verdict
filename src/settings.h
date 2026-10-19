#pragma once

#include <string>
#include <vector>
#include <map>

struct FlagInfo
{
    std::string name;
    std::vector<std::string> permissions;
};

struct TimeOption
{
    int seconds;
    std::string title;
};

struct ReasonOption
{
    std::string reason;
    std::vector<TimeOption> times;
};

struct NamedOption
{
    std::string value;
    std::string title;
};

struct EffectSettings
{
    std::string fireParticle;
    std::string explosionParticle;
    std::string igniteSound;
    std::string burnSound;
    std::string slapSound;
    std::string freezeSound;
    std::string bombTickSound;
    std::string bombLastTickSound;
    std::string explosionSound;
    int bombTicks;
    int freezeBombSeconds;
    int fireBombSeconds;
};

struct Settings
{
    std::string tablePrefix;
    int serverId;
    std::string serverAddress;
    std::string chatPrefix;
    int punishScope;
    bool checkIpOnSteamBans;
    bool storeIp;
    int timeReasonType;
    int notifyType;
    int immunityType;
    int banDelay;
    int unpunishType;
    int offlinePunishCount;
    int offlineUnpunishCount;
    int messageType;
    bool staticNames;
    bool muteIgnoreCommands;
    bool ownReasons;
    std::vector<std::string> defaultPermissions;
    std::map<std::string, FlagInfo> flags;
    std::vector<std::string> reasons[4];
    std::vector<TimeOption> times[4];
    std::vector<ReasonOption> reasonTimes[4];
    std::vector<NamedOption> maps;
    std::vector<NamedOption> configs;
    int mapChangeDelay;
    int voteDuration;
    int votePercent;
    int voteBanMinutes;
    int voteMuteMinutes;
    EffectSettings effects;
    std::vector<std::string> categoryOrder;
    std::map<std::string, std::vector<std::string>> itemOrder;
};

extern Settings g_Settings;

bool LoadSettings();
const char* PunishSection(int type);
