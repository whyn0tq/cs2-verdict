#include "admins.h"
#include "api.h"
#include "clients.h"
#include "phrases.h"
#include "settings.h"
#include "steam.h"
#include "storage.h"
#include "common.h"
#include <algorithm>

AdminInfo g_Admins[MAX_SLOTS];

static bool Contains(const std::vector<std::string>& list, const std::string& value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}

void ResetAdmin(int slot)
{
    if (!IsValidSlot(slot))
        return;

    AdminInfo& admin = g_Admins[slot];
    admin.id = 0;
    admin.name.clear();
    admin.groupId = -1;
    admin.groupName.clear();
    admin.immunity = 0;
    admin.expires = -1;
    admin.maxBanTime = -1;
    admin.maxMuteTime = -1;
    admin.flags.clear();
    admin.permissions.clear();
}

std::vector<std::string> PermissionsForFlag(const std::string& flag)
{
    std::map<std::string, FlagInfo>::iterator found = g_Settings.flags.find(flag);
    if (found == g_Settings.flags.end())
        return std::vector<std::string>();
    return found->second.permissions;
}

void GrantFlag(AdminInfo& admin, const std::string& flag)
{
    if (!Contains(admin.flags, flag))
        admin.flags.push_back(flag);

    std::vector<std::string> permissions = PermissionsForFlag(flag);
    for (size_t i = 0; i < permissions.size(); i++)
    {
        if (!Contains(admin.permissions, permissions[i]))
            admin.permissions.push_back(permissions[i]);
    }
}

static void GrantFlagLetters(AdminInfo& admin, const std::string& letters)
{
    for (size_t i = 0; i < letters.length(); i++)
    {
        if (letters[i] == ' ')
            continue;
        GrantFlag(admin, std::string(1, letters[i]));
    }
}

static void ApplyAdminRow(int slot, ISQLResult* result)
{
    AdminInfo admin;
    admin.id = result->GetInt(0);
    admin.name = ReadString(result, 1);
    admin.immunity = ReadInt(result, 3, 0);
    admin.expires = ReadInt(result, 4, 0);
    admin.groupId = ReadInt(result, 5, -1);
    admin.groupName = ReadString(result, 6);
    admin.maxBanTime = ReadInt(result, 9, -1);
    admin.maxMuteTime = ReadInt(result, 10, -1);

    if (admin.name.empty())
        admin.name = ClientName(slot);

    int groupImmunity = ReadInt(result, 8, 0);
    if (groupImmunity > admin.immunity)
        admin.immunity = groupImmunity;

    GrantFlagLetters(admin, ReadString(result, 2));
    GrantFlagLetters(admin, ReadString(result, 7));

    g_Admins[slot] = admin;

    if (g_Settings.staticNames && !admin.name.empty())
        g_pPlayers->SetPlayerName(slot, admin.name.c_str());

    g_pApi->NotifyAdminConnected(slot);
}

void LoadAdmin(int slot, uint64 steamId)
{
    ResetAdmin(slot);

    std::string query =
        "SELECT a.`aid`, a.`user`, a.`srv_flags`, a.`immunity`, a.`expired`, g.`id`, g.`name`, g.`flags`, g.`immunity`, g.`maxbantime`, g.`maxmutetime` "
        "FROM " + Table("admins") + " a LEFT JOIN " + Table("srvgroups") + " g ON g.`name` = a.`srv_group` "
        "WHERE a.`authid` REGEXP " + AuthRegexp(steamId) +
        " AND (a.`expired` IS NULL OR a.`expired` = 0 OR a.`expired` > UNIX_TIMESTAMP())" +
        AdminServerFilter("a.`aid`") + " LIMIT 1";

    RunQuery(query, [slot, steamId](ISQLQuery* query) {
        ISQLResult* result = ResultOf(query);
        if (!result || !IsSameClient(slot, steamId))
            return;
        if (result->FetchRow())
            ApplyAdminRow(slot, result);
    });
}

void CheckAdminExpiration()
{
    int now = Now();
    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        AdminInfo& admin = g_Admins[slot];
        if (admin.flags.empty() || admin.expires <= 0 || admin.expires > now)
            continue;

        ResetAdmin(slot);
        ChatTo(slot, Text("AdminExpired"));
    }
}

bool HasPermission(int slot, const char* permission)
{
    if (slot == CONSOLE_SLOT)
        return true;
    if (!IsValidSlot(slot) || !permission)
        return false;
    if (Contains(g_Settings.defaultPermissions, permission))
        return true;

    const std::vector<std::string>& permissions = g_Admins[slot].permissions;
    return Contains(permissions, permission) || Contains(permissions, "@admin/root");
}

bool HasFlag(int slot, const char* flag)
{
    if (slot == CONSOLE_SLOT)
        return true;
    if (!IsValidSlot(slot) || !flag)
        return false;
    return Contains(g_Admins[slot].flags, flag);
}

bool IsAdmin(int slot)
{
    if (slot == CONSOLE_SLOT)
        return true;
    if (!IsValidSlot(slot))
        return false;
    return !g_Admins[slot].flags.empty();
}

int AdminImmunity(int slot)
{
    if (!IsValidSlot(slot))
        return 0;
    return g_Admins[slot].immunity;
}

int AdminDatabaseId(int slot)
{
    if (!IsValidSlot(slot))
        return 0;
    return g_Admins[slot].id;
}

std::string AdminName(int slot)
{
    if (slot == CONSOLE_SLOT)
        return Text("Console");
    if (!IsValidSlot(slot))
        return "";
    if (g_Admins[slot].id > 0 && !g_Admins[slot].name.empty())
        return g_Admins[slot].name;
    return ClientName(slot);
}

std::string AdminDisplayName(int slot)
{
    if (g_Settings.messageType == 1)
        return Text("Admin");
    return AdminName(slot);
}

bool CanTargetImmunity(int adminSlot, int targetImmunity)
{
    if (adminSlot == CONSOLE_SLOT)
        return true;

    int adminImmunity = AdminImmunity(adminSlot);
    switch (g_Settings.immunityType)
    {
    case 1:
        return adminImmunity >= targetImmunity;
    case 2:
        return adminImmunity > targetImmunity;
    case 3:
        return targetImmunity <= 0;
    }
    return true;
}

bool CanTarget(int adminSlot, int targetSlot)
{
    if (adminSlot == CONSOLE_SLOT || adminSlot == targetSlot)
        return true;
    if (g_Settings.immunityType == 3 && IsAdmin(targetSlot))
        return false;
    return CanTargetImmunity(adminSlot, AdminImmunity(targetSlot));
}

bool CanUseDuration(int adminSlot, int type, int seconds)
{
    if (!IsValidSlot(adminSlot) || HasPermission(adminSlot, "@admin/root"))
        return true;

    int limitMinutes = type == RT_BAN ? g_Admins[adminSlot].maxBanTime : g_Admins[adminSlot].maxMuteTime;
    if (limitMinutes <= 0)
        return true;
    return seconds > 0 && seconds <= limitMinutes * 60;
}

struct NewAdmin
{
    std::string name;
    uint64 steamId;
    std::string flags;
    int immunity;
    int expires;
    int groupId;
    std::string comment;
};

static std::string GroupNameQuery(int groupId)
{
    if (groupId <= 0)
        return "NULL";
    return "(SELECT `name` FROM " + Table("srvgroups") + " WHERE `id` = " + std::to_string(groupId) + " LIMIT 1)";
}

static void ReloadOnlineAdmin(uint64 steamId)
{
    int slot = FindClientBySteamId(steamId);
    if (slot != -1)
        LoadAdmin(slot, steamId);
}

static void BindAdminToServer(int adminId, const NewAdmin& admin)
{
    std::string serverId = std::to_string(ServerId() > 0 ? ServerId() : -1);
    std::string adminIdText = std::to_string(adminId);

    RunQuery("DELETE FROM " + Table("admins_servers_groups") + " WHERE `admin_id` = " + adminIdText + " AND `server_id` = " + serverId);
    RunQuery("INSERT INTO " + Table("admins_servers_groups") + " (`admin_id`, `group_id`, `srv_group_id`, `server_id`) VALUES (" +
        adminIdText + ", " + std::to_string(admin.groupId > 0 ? admin.groupId : 0) + ", -1, " + serverId + ")",
        [admin](ISQLQuery*) { ReloadOnlineAdmin(admin.steamId); });
}

static void UpdateExistingAdmin(int adminId, const NewAdmin& admin)
{
    std::string query = "UPDATE " + Table("admins") + " SET `srv_flags` = " + Quote(admin.flags) +
        ", `immunity` = " + std::to_string(admin.immunity) +
        ", `expired` = " + std::to_string(admin.expires);
    if (admin.groupId > 0)
        query += ", `srv_group` = " + GroupNameQuery(admin.groupId);
    if (!admin.comment.empty())
        query += ", `comment` = " + Quote(admin.comment);
    query += " WHERE `aid` = " + std::to_string(adminId);

    RunQuery(query, [adminId, admin](ISQLQuery*) { BindAdminToServer(adminId, admin); });
}

static void InsertNewAdmin(const NewAdmin& admin, const std::string& login)
{
    std::string query = "INSERT INTO " + Table("admins") +
        " (`user`, `authid`, `password`, `gid`, `email`, `extraflags`, `immunity`, `srv_group`, `srv_flags`, `expired`, `comment`) VALUES (" +
        Quote(login) + ", " + Quote(SteamIdToSteam2(admin.steamId)) + ", '', -1, '', 0, " + std::to_string(admin.immunity) + ", " +
        GroupNameQuery(admin.groupId) + ", " + Quote(admin.flags) + ", " + std::to_string(admin.expires) + ", " + Quote(admin.comment) + ")";

    RunQuery(query, [admin](ISQLQuery* query) {
        if (!query)
            return;
        BindAdminToServer((int)query->GetInsertId(), admin);
    });
}

static void ChooseLoginAndInsert(const NewAdmin& admin)
{
    std::string query = "SELECT COUNT(*) FROM " + Table("admins") + " WHERE `user` = " + Quote(admin.name);
    RunQuery(query, [admin](ISQLQuery* query) {
        ISQLResult* result = ResultOf(query);
        if (!result || !result->FetchRow())
            return;

        std::string login = admin.name;
        if (result->GetInt(0) > 0)
            login = admin.name + " (" + SteamIdToSteam2(admin.steamId) + ")";
        InsertNewAdmin(admin, login);
    });
}

static void ApplyLocalAdmin(const NewAdmin& admin)
{
    int slot = FindClientBySteamId(admin.steamId);
    if (slot == -1)
        return;

    ResetAdmin(slot);
    AdminInfo& info = g_Admins[slot];
    info.name = admin.name;
    info.immunity = admin.immunity;
    info.expires = admin.expires;
    info.groupId = admin.groupId;
    GrantFlagLetters(info, admin.flags);
    g_pApi->NotifyAdminConnected(slot);
}

void AddAdmin(const std::string& name, uint64 steamId, const std::string& flags, int immunity, int expireSeconds, int groupId, const std::string& comment, bool writeToDatabase)
{
    if (steamId == 0)
        return;

    NewAdmin admin;
    admin.name = CleanName(name);
    admin.steamId = steamId;
    admin.flags = flags;
    admin.immunity = immunity;
    admin.expires = expireSeconds > 0 ? Now() + expireSeconds : 0;
    admin.groupId = groupId;
    admin.comment = CleanText(comment, 120);

    if (!writeToDatabase)
    {
        ApplyLocalAdmin(admin);
        return;
    }

    std::string query = "SELECT `aid` FROM " + Table("admins") + " WHERE `authid` REGEXP " + AuthRegexp(steamId) + " LIMIT 1";
    RunQuery(query, [admin](ISQLQuery* query) {
        ISQLResult* result = ResultOf(query);
        if (!result)
            return;
        if (result->FetchRow())
            UpdateExistingAdmin(result->GetInt(0), admin);
        else
            ChooseLoginAndInsert(admin);
    });
}

void RemoveAdmin(uint64 steamId, bool writeToDatabase)
{
    if (writeToDatabase)
    {
        std::string adminIds = "SELECT `aid` FROM " + Table("admins") + " WHERE `authid` REGEXP " + AuthRegexp(steamId);
        std::string query = "DELETE FROM " + Table("admins_servers_groups") + " WHERE `admin_id` IN (" + adminIds + ")";
        if (ServerId() > 0)
            query += " AND `server_id` IN (" + std::to_string(ServerId()) + ", -1)";
        RunQuery(query);

        RunQuery("DELETE FROM " + Table("admins") + " WHERE `authid` REGEXP " + AuthRegexp(steamId) +
            " AND `password` = '' AND `aid` NOT IN (SELECT `admin_id` FROM " + Table("admins_servers_groups") + ")");
    }

    int slot = FindClientBySteamId(steamId);
    if (slot != -1)
        ResetAdmin(slot);
}

void AddGroup(int slot, bool fromChat, const std::string& name, const std::string& flags, int immunity)
{
    std::string query = "INSERT INTO " + Table("srvgroups") +
        " (`flags`, `immunity`, `name`, `groups_immune`, `maxbantime`, `maxmutetime`) VALUES (" +
        Quote(flags) + ", " + std::to_string(immunity < 0 ? 0 : immunity) + ", " + Quote(CleanText(name, 120)) + ", '', -1, -1)";

    RunQuery(query, [slot, fromChat](ISQLQuery* query) {
        if (!query)
            return;
        std::string message = Text("GroupAdded");
        Replace(message, "{id}", std::to_string(query->GetInsertId()));
        ReplyTo(slot, fromChat, message);
    });
}

void RemoveGroup(const std::string& identifier)
{
    if (IsDigits(identifier))
        RunQuery("DELETE FROM " + Table("srvgroups") + " WHERE `id` = " + identifier);
    else
        RunQuery("DELETE FROM " + Table("srvgroups") + " WHERE `name` = " + Quote(identifier));
}
