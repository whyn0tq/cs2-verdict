#pragma once

#include <string>
#include <vector>

typedef unsigned long long uint64;

struct AdminInfo
{
    int id;
    std::string name;
    int groupId;
    std::string groupName;
    int immunity;
    int expires;
    int maxBanTime;
    int maxMuteTime;
    std::vector<std::string> flags;
    std::vector<std::string> permissions;
};

extern AdminInfo g_Admins[64];

void ResetAdmin(int slot);
void LoadAdmin(int slot, uint64 steamId);
void CheckAdminExpiration();

bool HasPermission(int slot, const char* permission);
bool HasFlag(int slot, const char* flag);
bool IsAdmin(int slot);
int AdminImmunity(int slot);
int AdminDatabaseId(int slot);
std::string AdminName(int slot);
std::string AdminDisplayName(int slot);
std::vector<std::string> PermissionsForFlag(const std::string& flag);
void GrantFlag(AdminInfo& admin, const std::string& flag);

bool CanTargetImmunity(int adminSlot, int targetImmunity);
bool CanTarget(int adminSlot, int targetSlot);
bool CanUseDuration(int adminSlot, int type, int seconds);

void AddAdmin(const std::string& name, uint64 steamId, const std::string& flags, int immunity, int expireSeconds, int groupId, const std::string& comment, bool writeToDatabase);
void RemoveAdmin(uint64 steamId, bool writeToDatabase);
void AddGroup(int slot, bool fromChat, const std::string& name, const std::string& flags, int immunity);
void RemoveGroup(const std::string& identifier);
