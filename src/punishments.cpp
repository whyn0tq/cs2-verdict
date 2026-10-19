#include "punishments.h"
#include "admins.h"
#include "api.h"
#include "clients.h"
#include "phrases.h"
#include "settings.h"
#include "steam.h"
#include "storage.h"
#include "common.h"

Punishment g_Punishments[MAX_SLOTS][4];

static const char* g_TypeKeys[4] = { "Ban", "Mute", "Gag", "Silence" };

typedef std::function<void(int affected)> RemovalCallback;

const char* TypeKey(int type)
{
    if (type < RT_BAN || type > RT_SILENCE)
        return "Ban";
    return g_TypeKeys[type];
}

static std::string PhraseKey(const char* group, int type)
{
    return std::string(group) + "_" + TypeKey(type);
}

static bool IsActive(const Punishment& punishment)
{
    if (punishment.expires == -1)
        return false;
    return punishment.expires == 0 || punishment.expires > Now();
}

static int LongerOf(int first, int second)
{
    if (first == 0 || second == 0)
        return 0;
    return first > second ? first : second;
}

static void ClearPunishment(Punishment& punishment)
{
    punishment.expires = -1;
    punishment.reason.clear();
    punishment.adminId = 0;
}

void ResetPunishments(int slot)
{
    if (!IsValidSlot(slot))
        return;
    for (int type = RT_BAN; type <= RT_SILENCE; type++)
        ClearPunishment(g_Punishments[slot][type]);
}

bool IsPunished(int slot, int type)
{
    if (!IsValidSlot(slot) || type < RT_BAN || type > RT_SILENCE)
        return false;
    return IsActive(g_Punishments[slot][type]);
}

int ActiveVoiceType(int slot)
{
    if (IsPunished(slot, RT_MUTE))
        return RT_MUTE;
    if (IsPunished(slot, RT_SILENCE))
        return RT_SILENCE;
    return -1;
}

int ActiveChatType(int slot)
{
    if (IsPunished(slot, RT_GAG))
        return RT_GAG;
    if (IsPunished(slot, RT_SILENCE))
        return RT_SILENCE;
    return -1;
}

bool IsVoiceBlocked(int slot)
{
    return ActiveVoiceType(slot) != -1;
}

bool IsChatBlocked(int slot)
{
    return ActiveChatType(slot) != -1;
}

static void AppendTimePart(std::string& result, int value, const char* key)
{
    if (value <= 0)
        return;
    if (!result.empty())
        result += " ";
    result += std::to_string(value) + Text(key);
}

std::string FormatDuration(int seconds)
{
    if (seconds <= 0)
        return Text("Forever");

    std::string result;
    AppendTimePart(result, seconds / 86400, "Days");
    AppendTimePart(result, (seconds % 86400) / 3600, "Hours");
    AppendTimePart(result, (seconds % 3600) / 60, "Minutes");
    if (result.empty())
        AppendTimePart(result, seconds % 60, "Seconds");
    return result;
}

std::string FormatTimeLeft(int expires)
{
    if (expires == 0)
        return Text("Forever");
    int left = expires - Now();
    if (left <= 0)
        return Text("Expired");
    return FormatDuration(left);
}

static std::string ReasonOrDefault(const std::string& reason)
{
    std::string clean = CleanText(reason, 250);
    if (clean.empty())
        clean = Text("NoReason");
    return clean;
}

void KickClient(int slot, const std::string& reason, bool banned)
{
    if (!IsValidSlot(slot))
        return;
    ENetworkDisconnectionReason code = banned ? NETWORK_DISCONNECT_KICKBANADDED : NETWORK_DISCONNECT_KICKED;
    std::string text = StripColors(reason);
    engine->DisconnectClient(CPlayerSlot(slot), code, text.c_str());
}

static std::string BanMessage(const std::string& reason, int seconds)
{
    std::string message = Text("BannedKick");
    Replace(message, "{reason}", reason);
    Replace(message, "{time}", FormatDuration(seconds));
    return message;
}

static void RejectBannedClient(int slot, int banId, const std::string& reason, int length, int ends)
{
    RunQuery("INSERT IGNORE INTO " + Table("banlog") + " (`sid`, `time`, `name`, `bid`) VALUES (" +
        std::to_string(ServerId()) + ", UNIX_TIMESTAMP(), " + Quote(ClientName(slot)) + ", " + std::to_string(banId) + ")");

    int secondsLeft = length == 0 ? 0 : ends - Now();
    std::string message = BanMessage(reason, secondsLeft);
    META_CONPRINTF("[Verdict] %s is banned (ban #%d), kicking\n", ClientName(slot).c_str(), banId);
    ConsoleTo(slot, message);
    KickClient(slot, message, true);
}

void CheckBan(int slot, uint64 steamId)
{
    std::string ip = ClientIp(slot);
    std::string ipCondition;
    if (!ip.empty())
    {
        ipCondition = " OR (b.`type` = 1 AND b.`ip` = " + Quote(ip) + ")";
        if (g_Settings.checkIpOnSteamBans)
            ipCondition += " OR (b.`type` = 0 AND b.`ip` = " + Quote(ip) + ")";
    }

    std::string query = "SELECT b.`bid`, b.`reason`, b.`length`, b.`ends` FROM " + Table("bans") + " b "
        "WHERE ((b.`type` = 0 AND b.`authid` REGEXP " + AuthRegexp(steamId) + ")" + ipCondition + ") "
        "AND (b.`length` = 0 OR b.`ends` > UNIX_TIMESTAMP()) AND b.`RemoveType` IS NULL" +
        PunishServerFilter("b.`sid`") + " ORDER BY b.`length` = 0 DESC, b.`ends` DESC LIMIT 1";

    RunQuery(query, [slot, steamId](ISQLQuery* query) {
        ISQLResult* result = ResultOf(query);
        if (!result || !IsSameClient(slot, steamId) || !result->FetchRow())
            return;
        RejectBannedClient(slot, result->GetInt(0), ReadString(result, 1), result->GetInt(2), result->GetInt(3));
    });
}

static void ApplyCommRow(int slot, ISQLResult* result)
{
    int type = result->GetInt(0);
    if (type < RT_MUTE || type > RT_SILENCE)
        return;

    int length = result->GetInt(1);
    int expires = length == 0 ? 0 : result->GetInt(2);

    Punishment& punishment = g_Punishments[slot][type];
    if (punishment.expires != -1)
    {
        int longer = LongerOf(punishment.expires, expires);
        if (longer == punishment.expires)
            return;
    }

    punishment.expires = expires;
    punishment.reason = ReadString(result, 3);
    punishment.adminId = ReadInt(result, 4, 0);
}

void LoadComms(int slot, uint64 steamId, bool announce)
{
    std::string query = "SELECT c.`type`, c.`length`, c.`ends`, c.`reason`, c.`aid` FROM " + Table("comms") + " c "
        "WHERE c.`authid` REGEXP " + AuthRegexp(steamId) + " AND c.`RemoveType` IS NULL "
        "AND (c.`length` = 0 OR c.`ends` > UNIX_TIMESTAMP())" + PunishServerFilter("c.`sid`");

    RunQuery(query, [slot, steamId, announce](ISQLQuery* query) {
        ISQLResult* result = ResultOf(query);
        if (!result || !IsSameClient(slot, steamId))
            return;

        while (result->FetchRow())
            ApplyCommRow(slot, result);

        if (!announce)
            return;

        for (int type = RT_MUTE; type <= RT_SILENCE; type++)
        {
            Punishment& punishment = g_Punishments[slot][type];
            if (!IsActive(punishment))
                continue;
            int seconds = punishment.expires == 0 ? 0 : punishment.expires - Now();
            g_pApi->NotifyPunish(slot, type, seconds, punishment.reason.c_str(), CONSOLE_SLOT);
        }
    });
}

void CheckExpiredPunishments()
{
    int now = Now();
    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (!IsRealClient(slot))
            continue;

        for (int type = RT_MUTE; type <= RT_SILENCE; type++)
        {
            Punishment& punishment = g_Punishments[slot][type];
            if (punishment.expires <= 0 || punishment.expires > now)
                continue;

            ClearPunishment(punishment);
            ChatTo(slot, Text("MuteExpired"));
            g_pApi->NotifyUnpunish(slot, type, CONSOLE_SLOT);
        }
    }
}

static std::string AdminIp(int adminSlot)
{
    if (!IsValidSlot(adminSlot))
        return "";
    return ClientIp(adminSlot);
}

static void InsertBan(uint64 steamId, const std::string& ip, const std::string& name, int seconds, const std::string& reason, int adminSlot)
{
    int type = steamId != 0 ? 0 : 1;
    std::string authid = steamId != 0 ? SteamIdToSteam2(steamId) : "";
    std::string storedIp = (g_Settings.storeIp || type == 1) ? ip : "";

    RunQuery("INSERT INTO " + Table("bans") +
        " (`type`, `ip`, `authid`, `name`, `created`, `ends`, `length`, `reason`, `aid`, `adminIp`, `sid`, `country`) VALUES (" +
        std::to_string(type) + ", " + Quote(storedIp) + ", " + Quote(authid) + ", " + Quote(CleanName(name)) + ", UNIX_TIMESTAMP(), UNIX_TIMESTAMP() + " +
        std::to_string(seconds) + ", " + std::to_string(seconds) + ", " + Quote(reason) + ", " + std::to_string(AdminDatabaseId(adminSlot)) + ", " +
        Quote(AdminIp(adminSlot)) + ", " + std::to_string(ServerId()) + ", ' ')");
}

static void InsertComm(uint64 steamId, const std::string& name, int type, int seconds, const std::string& reason, int adminSlot)
{
    RunQuery("INSERT INTO " + Table("comms") +
        " (`authid`, `name`, `created`, `ends`, `length`, `reason`, `aid`, `adminIp`, `sid`, `type`) VALUES (" +
        Quote(SteamIdToSteam2(steamId)) + ", " + Quote(CleanName(name)) + ", UNIX_TIMESTAMP(), UNIX_TIMESTAMP() + " + std::to_string(seconds) + ", " +
        std::to_string(seconds) + ", " + Quote(reason) + ", " + std::to_string(AdminDatabaseId(adminSlot)) + ", " + Quote(AdminIp(adminSlot)) + ", " +
        std::to_string(ServerId()) + ", " + std::to_string(type) + ")");
}

static void AnnouncePunishment(int targetSlot, int type, int seconds, const std::string& reason, int adminSlot)
{
    std::string duration = FormatDuration(seconds);
    std::string targetName = ClientName(targetSlot);
    std::string adminName = AdminDisplayName(adminSlot);

    META_CONPRINTF("[Verdict] %s: %s -> %s (%s, %s)\n", TypeKey(type), AdminName(adminSlot).c_str(), targetName.c_str(), duration.c_str(), reason.c_str());

    if (g_Settings.notifyType == 1)
    {
        std::string toTarget = Text(PhraseKey("Punish_Target", type).c_str());
        Replace(toTarget, "{admin}", adminName);
        Replace(toTarget, "{time}", duration);
        Replace(toTarget, "{reason}", reason);
        ChatTo(targetSlot, toTarget);

        if (IsValidSlot(adminSlot) && adminSlot != targetSlot)
        {
            std::string toAdmin = Text(PhraseKey("Punish_Admin", type).c_str());
            Replace(toAdmin, "{target}", targetName);
            Replace(toAdmin, "{time}", duration);
            Replace(toAdmin, "{reason}", reason);
            ChatTo(adminSlot, toAdmin);
        }
    }
    else if (g_Settings.notifyType == 2)
    {
        std::string toAll = Text(PhraseKey("Punish_All", type).c_str());
        Replace(toAll, "{admin}", adminName);
        Replace(toAll, "{target}", targetName);
        Replace(toAll, "{time}", duration);
        Replace(toAll, "{reason}", reason);
        ChatToAll(toAll);
    }
}

static void AnnounceUnpunish(int targetSlot, int type, int adminSlot)
{
    std::string targetName = ClientName(targetSlot);
    META_CONPRINTF("[Verdict] Un%s: %s -> %s\n", TypeKey(type), AdminName(adminSlot).c_str(), targetName.c_str());

    if (g_Settings.notifyType == 2)
    {
        std::string toAll = Text(PhraseKey("Unpunish_All", type).c_str());
        Replace(toAll, "{admin}", AdminDisplayName(adminSlot));
        Replace(toAll, "{target}", targetName);
        ChatToAll(toAll);
    }
    else if (g_Settings.notifyType == 1)
    {
        std::string toTarget = Text(PhraseKey("Unpunish_Target", type).c_str());
        Replace(toTarget, "{admin}", AdminDisplayName(adminSlot));
        ChatTo(targetSlot, toTarget);
    }
}

static void ScheduleBanKick(int slot, const std::string& reason, int seconds)
{
    uint64 steamId = ClientSteamId(slot);
    std::string message = BanMessage(reason, seconds);

    if (g_Settings.banDelay <= 0)
    {
        KickClient(slot, message, true);
        return;
    }

    g_pUtils->CreateTimer((float)g_Settings.banDelay, [slot, steamId, message]() {
        if (IsSameClient(slot, steamId))
            KickClient(slot, message, true);
        return -1.0f;
    });
}

bool PunishPlayer(int targetSlot, int type, int seconds, const std::string& reason, int adminSlot, bool notify, bool writeToDatabase)
{
    if (!IsRealClient(targetSlot) || type < RT_BAN || type > RT_SILENCE)
        return false;
    if (seconds < 0)
        seconds = 0;

    std::string cleanReason = ReasonOrDefault(reason);
    if (g_pApi->AskPunish(targetSlot, type, seconds, cleanReason.c_str(), adminSlot))
        return false;

    uint64 steamId = ClientSteamId(targetSlot);
    if (writeToDatabase)
    {
        if (type == RT_BAN)
            InsertBan(steamId, ClientIp(targetSlot), ClientName(targetSlot), seconds, cleanReason, adminSlot);
        else
            InsertComm(steamId, ClientName(targetSlot), type, seconds, cleanReason, adminSlot);
    }

    Punishment& punishment = g_Punishments[targetSlot][type];
    punishment.expires = seconds == 0 ? 0 : Now() + seconds;
    punishment.reason = cleanReason;
    punishment.adminId = AdminDatabaseId(adminSlot);

    if (notify)
        AnnouncePunishment(targetSlot, type, seconds, cleanReason, adminSlot);

    g_pApi->NotifyPunish(targetSlot, type, seconds, cleanReason.c_str(), adminSlot);

    if (type == RT_BAN)
        ScheduleBanKick(targetSlot, cleanReason, seconds);
    return true;
}

void PunishOffline(uint64 steamId, const std::string& name, int type, int seconds, const std::string& reason, int adminSlot)
{
    if (steamId == 0 || type < RT_BAN || type > RT_SILENCE)
        return;
    if (seconds < 0)
        seconds = 0;

    int online = FindClientBySteamId(steamId);
    if (online != -1)
    {
        PunishPlayer(online, type, seconds, reason, adminSlot, true, true);
        return;
    }

    std::string steamText = SteamIdToText(steamId);
    std::string cleanReason = ReasonOrDefault(reason);
    RecentClient* recent = FindRecentClient(steamId);

    std::string cleanName = CleanName(name);
    if ((name.empty() || name == "undefined") && recent)
        cleanName = recent->name;

    if (g_pApi->AskOfflinePunish(steamText.c_str(), cleanName.c_str(), type, seconds, cleanReason.c_str(), adminSlot))
        return;

    std::string ip = recent ? recent->ip : "";
    if (recent)
        recent->punished[type] = true;

    if (type == RT_BAN)
        InsertBan(steamId, ip, cleanName, seconds, cleanReason, adminSlot);
    else
        InsertComm(steamId, cleanName, type, seconds, cleanReason, adminSlot);

    META_CONPRINTF("[Verdict] Offline %s: %s -> %s (%s)\n", TypeKey(type), AdminName(adminSlot).c_str(), SteamIdToSteam2(steamId).c_str(), cleanReason.c_str());
    g_pApi->NotifyOfflinePunish(steamText.c_str(), cleanName.c_str(), type, seconds, cleanReason.c_str(), adminSlot);
}

void PunishOfflineChecked(uint64 steamId, const std::string& name, int type, int seconds, const std::string& reason, int adminSlot, bool fromChat)
{
    if (adminSlot == CONSOLE_SLOT || g_Settings.immunityType == 0)
    {
        PunishOffline(steamId, name, type, seconds, reason, adminSlot);
        ReplyTo(adminSlot, fromChat, Text("PlayerPunished"));
        return;
    }

    std::string query = "SELECT GREATEST(a.`immunity`, IFNULL(g.`immunity`, 0)) FROM " + Table("admins") + " a "
        "LEFT JOIN " + Table("srvgroups") + " g ON g.`name` = a.`srv_group` WHERE a.`authid` REGEXP " + AuthRegexp(steamId) + " LIMIT 1";

    RunQuery(query, [steamId, name, type, seconds, reason, adminSlot, fromChat](ISQLQuery* query) {
        ISQLResult* result = ResultOf(query);
        if (!result)
            return;

        bool targetIsAdmin = result->FetchRow();
        int immunity = targetIsAdmin ? result->GetInt(0) : 0;

        bool allowed = CanTargetImmunity(adminSlot, immunity);
        if (g_Settings.immunityType == 3 && targetIsAdmin)
            allowed = false;

        if (!allowed)
        {
            ReplyTo(adminSlot, fromChat, Text("TargetImmune"));
            return;
        }

        PunishOffline(steamId, name, type, seconds, reason, adminSlot);
        ReplyTo(adminSlot, fromChat, Text("PlayerPunished"));
    });
}

void BanIp(const std::string& ip, int seconds, const std::string& reason, int adminSlot)
{
    if (!LooksLikeIp(ip))
        return;
    if (seconds < 0)
        seconds = 0;

    std::string cleanReason = ReasonOrDefault(reason);
    InsertBan(0, ip, "unnamed", seconds, cleanReason, adminSlot);
    META_CONPRINTF("[Verdict] IP ban: %s -> %s (%s)\n", AdminName(adminSlot).c_str(), ip.c_str(), cleanReason.c_str());

    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (IsRealClient(slot) && ClientIp(slot) == ip)
            PunishPlayer(slot, RT_BAN, seconds, cleanReason, adminSlot, true, false);
    }
}

static bool UnpunishAllowedFor(int adminSlot)
{
    if (adminSlot == CONSOLE_SLOT || g_Settings.unpunishType == 1)
        return true;
    return HasPermission(adminSlot, "@admin/unpunish_all");
}

static std::string OwnPunishmentsOnly(int adminSlot)
{
    if (UnpunishAllowedFor(adminSlot))
        return "";
    return " AND `aid` = " + std::to_string(AdminDatabaseId(adminSlot));
}

static int RelevantType(int slot, int type)
{
    if (type == RT_MUTE)
        return ActiveVoiceType(slot);
    if (type == RT_GAG)
        return ActiveChatType(slot);
    if (type == RT_SILENCE)
    {
        if (IsPunished(slot, RT_SILENCE))
            return RT_SILENCE;
        if (IsPunished(slot, RT_MUTE))
            return RT_MUTE;
        if (IsPunished(slot, RT_GAG))
            return RT_GAG;
        return -1;
    }
    return IsPunished(slot, RT_BAN) ? RT_BAN : -1;
}

bool CanRemovePunishment(int adminSlot, int targetSlot, int type)
{
    int relevant = RelevantType(targetSlot, type);
    if (relevant == -1)
        return false;
    if (UnpunishAllowedFor(adminSlot))
        return true;

    int adminId = AdminDatabaseId(adminSlot);
    return adminId > 0 && g_Punishments[targetSlot][relevant].adminId == adminId;
}

static std::string RemovalFields(int adminSlot, const std::string& reason)
{
    return " SET `RemovedBy` = " + std::to_string(AdminDatabaseId(adminSlot)) +
        ", `RemoveType` = 'U', `RemovedOn` = UNIX_TIMESTAMP(), `ureason` = " + Quote(CleanText(reason, 250));
}

static std::string ActiveOnly()
{
    return " AND (`length` = 0 OR `ends` > UNIX_TIMESTAMP()) AND `RemoveType` IS NULL";
}

static void RemoveCommsInDatabase(uint64 steamId, int type, int adminSlot, const std::string& reason, const std::string& restriction, RemovalCallback done)
{
    std::string who = " WHERE `authid` REGEXP " + AuthRegexp(steamId) + ActiveOnly() + restriction;
    std::string removal = "UPDATE " + Table("comms") + RemovalFields(adminSlot, reason) + who;

    if (type == RT_SILENCE)
    {
        RunQuery(removal + " AND `type` IN (1, 2, 3)", [done](ISQLQuery* query) {
            done(query ? (int)query->GetAffectedRows() : 0);
        });
        return;
    }

    int remaining = type == RT_MUTE ? RT_GAG : RT_MUTE;
    std::string split = "UPDATE " + Table("comms") + " SET `type` = " + std::to_string(remaining) + who + " AND `type` = 3";
    std::string typed = removal + " AND `type` = " + std::to_string(type);

    RunQuery(split, [typed, done](ISQLQuery* splitQuery) {
        int splitRows = splitQuery ? (int)splitQuery->GetAffectedRows() : 0;
        RunQuery(typed, [splitRows, done](ISQLQuery* query) {
            done(splitRows + (query ? (int)query->GetAffectedRows() : 0));
        });
    });
}

static void RemoveBanInDatabase(uint64 steamId, int adminSlot, const std::string& reason, const std::string& restriction, RemovalCallback done)
{
    std::string query = "UPDATE " + Table("bans") + RemovalFields(adminSlot, reason) +
        " WHERE `type` = 0 AND `authid` REGEXP " + AuthRegexp(steamId) + ActiveOnly() + restriction;

    RunQuery(query, [done](ISQLQuery* query) {
        done(query ? (int)query->GetAffectedRows() : 0);
    });
}

static void ClearLocalPunishment(int slot, int type)
{
    Punishment* list = g_Punishments[slot];
    if (type == RT_MUTE || type == RT_GAG)
    {
        int remaining = type == RT_MUTE ? RT_GAG : RT_MUTE;
        if (IsActive(list[RT_SILENCE]))
        {
            if (IsActive(list[remaining]))
                list[remaining].expires = LongerOf(list[remaining].expires, list[RT_SILENCE].expires);
            else
                list[remaining] = list[RT_SILENCE];
            ClearPunishment(list[RT_SILENCE]);
        }
        ClearPunishment(list[type]);
        return;
    }

    if (type == RT_SILENCE)
    {
        ClearPunishment(list[RT_MUTE]);
        ClearPunishment(list[RT_GAG]);
        ClearPunishment(list[RT_SILENCE]);
        return;
    }

    ClearPunishment(list[type]);
}

bool UnpunishPlayer(int targetSlot, int type, int adminSlot, bool notify, const std::string& reason)
{
    if (!IsRealClient(targetSlot) || type < RT_BAN || type > RT_SILENCE)
        return false;
    if (!CanRemovePunishment(adminSlot, targetSlot, type))
        return false;

    uint64 steamId = ClientSteamId(targetSlot);
    if (type == RT_BAN)
        RemoveBanInDatabase(steamId, adminSlot, reason, "", [](int) {});
    else
        RemoveCommsInDatabase(steamId, type, adminSlot, reason, "", [](int) {});

    ClearLocalPunishment(targetSlot, type);

    if (notify)
        AnnounceUnpunish(targetSlot, type, adminSlot);
    g_pApi->NotifyUnpunish(targetSlot, type, adminSlot);
    return true;
}

void UnpunishOffline(uint64 steamId, int type, int adminSlot, const std::string& reason, bool fromChat)
{
    if (steamId == 0 || type < RT_BAN || type > RT_SILENCE)
        return;

    int online = FindClientBySteamId(steamId);
    if (online != -1 && type != RT_BAN)
    {
        bool removed = UnpunishPlayer(online, type, adminSlot, true, reason);
        ReplyTo(adminSlot, fromChat, Text(removed ? "PlayerUnpunished" : "NothingToRemove"));
        return;
    }

    RecentClient* recent = FindRecentClient(steamId);
    if (recent)
        recent->punished[type] = false;

    std::string steamText = SteamIdToText(steamId);
    RemovalCallback done = [steamText, type, adminSlot, fromChat](int affected) {
        if (affected <= 0)
        {
            ReplyTo(adminSlot, fromChat, Text("NothingToRemove"));
            return;
        }
        META_CONPRINTF("[Verdict] Offline un%s: %s -> %s\n", TypeKey(type), AdminName(adminSlot).c_str(), steamText.c_str());
        g_pApi->NotifyOfflineUnpunish(steamText.c_str(), type, adminSlot);
        ReplyTo(adminSlot, fromChat, Text("PlayerUnpunished"));
    };

    std::string restriction = OwnPunishmentsOnly(adminSlot);
    if (type == RT_BAN)
        RemoveBanInDatabase(steamId, adminSlot, reason, restriction, done);
    else
        RemoveCommsInDatabase(steamId, type, adminSlot, reason, restriction, done);
}

void UnbanIp(const std::string& ip, int adminSlot, const std::string& reason, bool fromChat)
{
    std::string query = "UPDATE " + Table("bans") + RemovalFields(adminSlot, reason) +
        " WHERE `type` = 1 AND `ip` = " + Quote(ip) + ActiveOnly() + OwnPunishmentsOnly(adminSlot);

    RunQuery(query, [ip, adminSlot, fromChat](ISQLQuery* query) {
        int affected = query ? (int)query->GetAffectedRows() : 0;
        if (affected <= 0)
        {
            ReplyTo(adminSlot, fromChat, Text("NothingToRemove"));
            return;
        }
        META_CONPRINTF("[Verdict] IP unban: %s -> %s\n", AdminName(adminSlot).c_str(), ip.c_str());
        ReplyTo(adminSlot, fromChat, Text("PlayerUnpunished"));
    });
}

void KickWithAnnounce(int targetSlot, int adminSlot, const std::string& reason)
{
    std::string cleanReason = ReasonOrDefault(reason);
    std::string targetName = ClientName(targetSlot);
    META_CONPRINTF("[Verdict] Kick: %s -> %s (%s)\n", AdminName(adminSlot).c_str(), targetName.c_str(), cleanReason.c_str());

    if (g_Settings.notifyType == 2)
    {
        std::string toAll = Text("Kick_All");
        Replace(toAll, "{admin}", AdminDisplayName(adminSlot));
        Replace(toAll, "{target}", targetName);
        Replace(toAll, "{reason}", cleanReason);
        ChatToAll(toAll);
    }

    std::string message = Text("KickedReason");
    Replace(message, "{reason}", cleanReason);
    KickClient(targetSlot, message, false);
}
