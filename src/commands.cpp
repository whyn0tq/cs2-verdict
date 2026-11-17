#include "commands.h"
#include "entities.h"
#include "player_actions.h"
#include "server_actions.h"
#include "votes.h"
#include "admin_menu.h"
#include "admins.h"
#include "clients.h"
#include "migration.h"
#include "phrases.h"
#include "plugin.h"
#include "punishments.h"
#include "settings.h"
#include "steam.h"
#include "common.h"

enum TargetKind
{
    TargetNone,
    TargetOnline,
    TargetSteam,
    TargetIp
};

struct Target
{
    TargetKind kind;
    int slot;
    uint64 steamId;
    std::string ip;
};

typedef void (*CommandHandler)(int caller, bool fromChat, const std::vector<std::string>& args);

struct CommandEntry
{
    const char* name;
    CommandHandler handler;
};

static std::string Lowercase(const std::string& text)
{
    std::string result = text;
    for (size_t i = 0; i < result.length(); i++)
        result[i] = (char)tolower((unsigned char)result[i]);
    return result;
}

static std::string TrimQuotes(const std::string& text)
{
    size_t start = 0;
    size_t end = text.length();
    while (start < end && (text[start] == '"' || text[start] == '\'' || isspace((unsigned char)text[start])))
        start++;
    while (end > start && (text[end - 1] == '"' || text[end - 1] == '\'' || isspace((unsigned char)text[end - 1])))
        end--;
    return text.substr(start, end - start);
}

static std::vector<std::string> SplitArguments(const CCommand& command)
{
    std::vector<std::string> args;
    for (int i = 0; i < command.ArgC(); i++)
    {
        std::string arg = TrimQuotes(command[i]);
        if (!arg.empty())
            args.push_back(arg);
    }
    return args;
}

static std::string JoinFrom(const std::vector<std::string>& args, size_t first)
{
    std::string result;
    for (size_t i = first; i < args.size(); i++)
    {
        if (!result.empty())
            result += " ";
        result += args[i];
    }
    return result;
}

static std::string CommandName(const std::vector<std::string>& args)
{
    if (args.empty())
        return "";
    return args[0];
}

static void ReplyUsage(int caller, bool fromChat, const char* key, const std::vector<std::string>& args)
{
    std::string message = Text(key);
    Replace(message, "{command}", CommandName(args));
    ReplyTo(caller, fromChat, message);
}

static bool CheckAccess(int caller, bool fromChat, const char* permission)
{
    if (HasPermission(caller, permission))
        return true;
    ReplyTo(caller, fromChat, Text("NoPermission"));
    return false;
}

int ParseDuration(const std::string& text)
{
    if (text.empty())
        return -1;

    std::string number = text;
    int multiplier = 60;
    char unit = (char)tolower((unsigned char)text[text.length() - 1]);
    if (unit < '0' || unit > '9')
    {
        number = text.substr(0, text.length() - 1);
        switch (unit)
        {
        case 's':
            multiplier = 1;
            break;
        case 'm':
            multiplier = 60;
            break;
        case 'h':
            multiplier = 3600;
            break;
        case 'd':
            multiplier = 86400;
            break;
        case 'w':
            multiplier = 604800;
            break;
        default:
            return -1;
        }
    }

    if (!IsDigits(number) || number.length() > 7)
        return -1;
    return atoi(number.c_str()) * multiplier;
}

static int FindByName(const std::string& text, int& matches)
{
    std::string needle = Lowercase(text);
    int found = -1;
    matches = 0;

    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (!IsRealClient(slot))
            continue;

        std::string name = Lowercase(ClientName(slot));
        if (name == needle)
        {
            matches = 1;
            return slot;
        }
        if (name.find(needle) != std::string::npos)
        {
            found = slot;
            matches++;
        }
    }
    return found;
}

static Target ResolveTarget(int caller, bool fromChat, const std::string& text)
{
    Target target;
    target.kind = TargetNone;
    target.slot = -1;
    target.steamId = 0;

    if (LooksLikeIp(text))
    {
        target.kind = TargetIp;
        target.ip = text;
        return target;
    }

    uint64 steamId = 0;
    if (ParseSteamId(text, steamId))
    {
        target.steamId = steamId;
        target.slot = FindClientBySteamId(steamId);
        target.kind = target.slot == -1 ? TargetSteam : TargetOnline;
        return target;
    }

    std::string slotText = text[0] == '#' ? text.substr(1) : text;
    if (IsDigits(slotText) && slotText.length() <= 2)
    {
        int slot = atoi(slotText.c_str());
        if (IsRealClient(slot))
        {
            target.kind = TargetOnline;
            target.slot = slot;
            target.steamId = ClientSteamId(slot);
            return target;
        }
    }

    int matches = 0;
    int slot = FindByName(text, matches);
    if (matches > 1)
    {
        ReplyTo(caller, fromChat, Text("MultiplePlayers"));
        return target;
    }
    if (slot == -1)
    {
        ReplyTo(caller, fromChat, Text("PlayerNotFound"));
        return target;
    }

    target.kind = TargetOnline;
    target.slot = slot;
    target.steamId = ClientSteamId(slot);
    return target;
}

static int TypeFromName(const std::string& name)
{
    if (name.find("silence") != std::string::npos)
        return RT_SILENCE;
    if (name.find("gag") != std::string::npos)
        return RT_GAG;
    if (name.find("mute") != std::string::npos)
        return RT_MUTE;
    return RT_BAN;
}

static const char* g_PunishPermissions[4] = { "@admin/ban", "@admin/mute", "@admin/gag", "@admin/silence" };
static const char* g_UnpunishPermissions[4] = { "@admin/unban", "@admin/unmute", "@admin/ungag", "@admin/unsilence" };

static void HandlePunish(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int type = TypeFromName(CommandName(args));
    if (!CheckAccess(caller, fromChat, g_PunishPermissions[type]))
        return;
    if (args.size() < 3)
    {
        ReplyUsage(caller, fromChat, "UsagePunish", args);
        return;
    }

    int seconds = ParseDuration(args[2]);
    if (seconds < 0)
    {
        ReplyTo(caller, fromChat, Text("InvalidTime"));
        return;
    }
    if (!CanUseDuration(caller, type, seconds))
    {
        ReplyTo(caller, fromChat, Text("TimeNotAllowed"));
        return;
    }

    std::string reason = JoinFrom(args, 3);
    Target target = ResolveTarget(caller, fromChat, args[1]);

    if (target.kind == TargetOnline)
    {
        if (!CanTarget(caller, target.slot))
        {
            ReplyTo(caller, fromChat, Text("TargetImmune"));
            return;
        }
        if (IsPunished(target.slot, type))
        {
            ReplyTo(caller, fromChat, Text("AlreadyPunished"));
            return;
        }
        bool done = PunishPlayer(target.slot, type, seconds, reason, caller, true, true);
        ReplyTo(caller, fromChat, Text(done ? "PlayerPunished" : "PunishBlocked"));
    }
    else if (target.kind == TargetSteam)
    {
        PunishOfflineChecked(target.steamId, "", type, seconds, reason, caller, fromChat);
    }
    else if (target.kind == TargetIp)
    {
        if (type != RT_BAN)
        {
            ReplyTo(caller, fromChat, Text("PlayerNotFound"));
            return;
        }
        BanIp(target.ip, seconds, reason, caller);
        ReplyTo(caller, fromChat, Text("PlayerPunished"));
    }
}

static void HandleUnpunish(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int type = TypeFromName(CommandName(args));
    if (!CheckAccess(caller, fromChat, g_UnpunishPermissions[type]))
        return;
    if (args.size() < 2)
    {
        ReplyUsage(caller, fromChat, "UsageUnpunish", args);
        return;
    }

    std::string reason = JoinFrom(args, 2);
    Target target = ResolveTarget(caller, fromChat, args[1]);

    if (target.kind == TargetIp)
    {
        if (type == RT_BAN)
            UnbanIp(target.ip, caller, reason, fromChat);
        else
            ReplyTo(caller, fromChat, Text("PlayerNotFound"));
        return;
    }

    if (target.kind == TargetOnline && type != RT_BAN)
    {
        bool done = UnpunishPlayer(target.slot, type, caller, true, reason);
        ReplyTo(caller, fromChat, Text(done ? "PlayerUnpunished" : "NothingToRemove"));
        return;
    }

    if (target.kind == TargetOnline || target.kind == TargetSteam)
        UnpunishOffline(target.steamId, type, caller, reason, fromChat);
}

static void HandleBanIp(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/ban"))
        return;
    if (args.size() < 3 || !LooksLikeIp(args[1]))
    {
        ReplyUsage(caller, fromChat, "UsageBanIp", args);
        return;
    }

    int seconds = ParseDuration(args[2]);
    if (seconds < 0)
    {
        ReplyTo(caller, fromChat, Text("InvalidTime"));
        return;
    }
    if (!CanUseDuration(caller, RT_BAN, seconds))
    {
        ReplyTo(caller, fromChat, Text("TimeNotAllowed"));
        return;
    }

    BanIp(args[1], seconds, JoinFrom(args, 3), caller);
    ReplyTo(caller, fromChat, Text("PlayerPunished"));
}

static void HandleKick(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/kick"))
        return;
    if (args.size() < 2)
    {
        ReplyUsage(caller, fromChat, "UsageKick", args);
        return;
    }

    Target target = ResolveTarget(caller, fromChat, args[1]);
    if (target.kind != TargetOnline)
    {
        if (target.kind != TargetNone)
            ReplyTo(caller, fromChat, Text("PlayerNotFound"));
        return;
    }
    if (!CanTarget(caller, target.slot))
    {
        ReplyTo(caller, fromChat, Text("TargetImmune"));
        return;
    }

    KickWithAnnounce(target.slot, caller, JoinFrom(args, 2));
    ReplyTo(caller, fromChat, Text("PlayerKicked"));
}

static bool CanGrantFlags(int caller, const std::string& flags)
{
    if (HasPermission(caller, "@admin/root"))
        return true;
    for (size_t i = 0; i < flags.length(); i++)
    {
        if (!HasFlag(caller, std::string(1, flags[i]).c_str()))
            return false;
    }
    return true;
}

static void HandleAddAdmin(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!HasPermission(caller, "@admin/add_admin") && !HasPermission(caller, "@admin/add"))
    {
        ReplyTo(caller, fromChat, Text("NoPermission"));
        return;
    }
    if (args.size() < 6)
    {
        ReplyUsage(caller, fromChat, "UsageAddAdmin", args);
        return;
    }

    Target target = ResolveTarget(caller, fromChat, args[1]);
    if (target.kind != TargetOnline && target.kind != TargetSteam)
        return;

    std::string flags = args[3];
    int immunity = atoi(args[4].c_str());
    int seconds = ParseDuration(args[5]);
    if (seconds < 0)
    {
        ReplyTo(caller, fromChat, Text("InvalidTime"));
        return;
    }
    if (!CanGrantFlags(caller, flags) || (caller != CONSOLE_SLOT && immunity > AdminImmunity(caller) && !HasPermission(caller, "@admin/root")))
    {
        ReplyTo(caller, fromChat, Text("CannotGrant"));
        return;
    }

    int groupId = args.size() > 6 ? atoi(args[6].c_str()) : 0;
    std::string comment = JoinFrom(args, 7);

    AddAdmin(args[2], target.steamId, flags, immunity, seconds, groupId, comment, true);
    ReplyTo(caller, fromChat, Text("AdminAdded"));
}

static void HandleRemoveAdmin(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!HasPermission(caller, "@admin/remove_admin") && !HasPermission(caller, "@admin/remove"))
    {
        ReplyTo(caller, fromChat, Text("NoPermission"));
        return;
    }
    if (args.size() < 2)
    {
        ReplyUsage(caller, fromChat, "UsageRemoveAdmin", args);
        return;
    }

    Target target = ResolveTarget(caller, fromChat, args[1]);
    if (target.kind != TargetOnline && target.kind != TargetSteam)
        return;
    if (target.kind == TargetOnline && !CanTarget(caller, target.slot))
    {
        ReplyTo(caller, fromChat, Text("TargetImmune"));
        return;
    }

    RemoveAdmin(target.steamId, true);
    ReplyTo(caller, fromChat, Text("AdminRemoved"));
}

static void HandleAddGroup(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/add_group"))
        return;
    if (args.size() < 4)
    {
        ReplyUsage(caller, fromChat, "UsageAddGroup", args);
        return;
    }
    if (!CanGrantFlags(caller, args[2]))
    {
        ReplyTo(caller, fromChat, Text("CannotGrant"));
        return;
    }
    AddGroup(caller, fromChat, args[1], args[2], atoi(args[3].c_str()));
}

static void HandleRemoveGroup(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/remove_group"))
        return;
    if (args.size() < 2)
    {
        ReplyUsage(caller, fromChat, "UsageRemoveGroup", args);
        return;
    }
    RemoveGroup(args[1]);
    ReplyTo(caller, fromChat, Text("GroupRemoved"));
}

static void HandleReloadConfig(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/reload_config"))
        return;
    LoadSettings();
    LoadPhrases();
    ReplyTo(caller, fromChat, Text("ConfigReloaded"));
}

static int OnlineSlotFromArgs(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (args.size() < 2)
    {
        ReplyUsage(caller, fromChat, "UsageSteamId", args);
        return -1;
    }
    Target target = ResolveTarget(caller, fromChat, args[1]);
    if (target.kind == TargetOnline)
        return target.slot;
    if (target.kind != TargetNone)
        ReplyTo(caller, fromChat, Text("PlayerNotFound"));
    return -1;
}

static void HandleReloadAdmin(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/reload_admin"))
        return;
    int slot = OnlineSlotFromArgs(caller, fromChat, args);
    if (slot == -1)
        return;
    LoadAdmin(slot, ClientSteamId(slot));
    ReplyTo(caller, fromChat, Text("PlayerReloaded"));
}

static void HandleReloadPunish(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/reload_punish"))
        return;
    int slot = OnlineSlotFromArgs(caller, fromChat, args);
    if (slot == -1)
        return;
    ResetPunishments(slot);
    CheckBan(slot, ClientSteamId(slot));
    LoadComms(slot, ClientSteamId(slot), true);
    ReplyTo(caller, fromChat, Text("PlayerReloaded"));
}

static void HandleSync(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/reload_admin"))
        return;

    if (args.size() < 2)
    {
        for (int slot = 0; slot < MAX_SLOTS; slot++)
        {
            if (IsRealClient(slot))
                SyncPlayer(slot);
        }
        ReplyTo(caller, fromChat, Text("EveryoneReloaded"));
        return;
    }

    uint64 steamId = 0;
    if (!ParseSteamId(args[1], steamId))
    {
        ReplyUsage(caller, fromChat, "UsageSteamId", args);
        return;
    }
    int slot = FindClientBySteamId(steamId);
    if (slot != -1)
        SyncPlayer(slot);
    ReplyTo(caller, fromChat, Text("PlayerReloaded"));
}

static void HandleImport(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (caller != CONSOLE_SLOT)
    {
        ReplyTo(caller, fromChat, Text("ConsoleOnly"));
        return;
    }
    ImportAdminSystem(args.size() > 1 ? args[1] : "as_");
}

static int TargetForAction(int caller, bool fromChat, const std::vector<std::string>& args, size_t minArgs, const char* usageKey, const char* permission)
{
    if (!CheckAccess(caller, fromChat, permission))
        return -1;
    if (args.size() < minArgs)
    {
        ReplyUsage(caller, fromChat, usageKey, args);
        return -1;
    }

    Target target = ResolveTarget(caller, fromChat, args[1]);
    if (target.kind != TargetOnline)
    {
        if (target.kind != TargetNone)
            ReplyTo(caller, fromChat, Text("PlayerNotFound"));
        return -1;
    }
    if (!CanTarget(caller, target.slot))
    {
        ReplyTo(caller, fromChat, Text("TargetImmune"));
        return -1;
    }
    return target.slot;
}

static void ReplyActionResult(int caller, bool fromChat, bool done)
{
    if (!done)
        ReplyTo(caller, fromChat, Text("ActionNotPossible"));
}

static void HandleSlay(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 2, "UsageTarget", "@admin/kill");
    if (target != -1)
        ReplyActionResult(caller, fromChat, SlayPlayer(target, caller));
}

static void HandleSlap(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 2, "UsageSlap", "@admin/slap");
    if (target == -1)
        return;
    int damage = args.size() > 2 ? atoi(args[2].c_str()) : 0;
    if (damage < 0)
        damage = 0;
    ReplyActionResult(caller, fromChat, SlapPlayer(target, caller, damage));
}

static void HandleRename(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 3, "UsageRename", "@admin/rename");
    if (target != -1)
        ReplyActionResult(caller, fromChat, RenamePlayer(target, caller, JoinFrom(args, 2)));
}

static int TeamFromText(const std::string& text)
{
    std::string team = Lowercase(text);
    if (team == "t" || team == "2")
        return TEAM_T;
    if (team == "ct" || team == "3")
        return TEAM_CT;
    if (team == "spec" || team == "spectator" || team == "1")
        return TEAM_SPECTATOR;
    return -1;
}

static void HandleTeam(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 3, "UsageTeam", "@admin/changeteam");
    if (target == -1)
        return;
    int team = TeamFromText(args[2]);
    if (team == -1)
    {
        ReplyUsage(caller, fromChat, "UsageTeam", args);
        return;
    }
    ReplyActionResult(caller, fromChat, MovePlayerToTeam(target, caller, team));
}

static void HandleNoclip(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 2, "UsageTarget", "@admin/noclip");
    if (target != -1)
        ReplyActionResult(caller, fromChat, ToggleNoclip(target, caller));
}

static void HandleFreeze(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 2, "UsageFreeze", "@admin/freeze");
    if (target == -1)
        return;
    int seconds = IsFrozen(target) ? 0 : 10;
    if (args.size() > 2)
        seconds = atoi(args[2].c_str());
    ReplyActionResult(caller, fromChat, FreezePlayer(target, caller, seconds));
}

static void HandleBomb(int caller, bool fromChat, const std::vector<std::string>& args, int type, const char* permission)
{
    int target = TargetForAction(caller, fromChat, args, 2, "UsageTarget", permission);
    if (target != -1)
        ReplyActionResult(caller, fromChat, ToggleBomb(target, caller, type));
}

static void HandleTimeBomb(int caller, bool fromChat, const std::vector<std::string>& args)
{
    HandleBomb(caller, fromChat, args, BOMB_TIME, "@admin/timebomb");
}

static void HandleFireBomb(int caller, bool fromChat, const std::vector<std::string>& args)
{
    HandleBomb(caller, fromChat, args, BOMB_FIRE, "@admin/firebomb");
}

static void HandleFreezeBomb(int caller, bool fromChat, const std::vector<std::string>& args)
{
    HandleBomb(caller, fromChat, args, BOMB_FREEZE, "@admin/freezebomb");
}

static void HandleRespawn(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 2, "UsageTarget", "@admin/respawn");
    if (target != -1)
        ReplyActionResult(caller, fromChat, RespawnPlayer(target, caller));
}

static void HandleWho(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/who"))
        return;
    if (args.size() < 2)
    {
        if (IsRealClient(caller))
            ShowPlayerInfo(caller, fromChat, caller);
        else
            ReplyUsage(caller, fromChat, "UsageTarget", args);
        return;
    }
    Target target = ResolveTarget(caller, fromChat, args[1]);
    if (target.kind == TargetOnline)
        ShowPlayerInfo(caller, fromChat, target.slot);
    else if (target.kind != TargetNone)
        ReplyTo(caller, fromChat, Text("PlayerNotFound"));
}

static void HandleMap(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/maps"))
        return;
    if (args.size() < 2)
    {
        ReplyUsage(caller, fromChat, "UsageMap", args);
        return;
    }
    ChangeMap(caller, args[1]);
}

static void HandleRestart(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (CheckAccess(caller, fromChat, "@admin/restart"))
        RestartGame(caller);
}

static void HandleSwapTeams(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (CheckAccess(caller, fromChat, "@admin/swapteams"))
        SwapTeams(caller);
}

static void HandleBurn(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 2, "UsageBurn", "@admin/burn");
    if (target == -1)
        return;
    int seconds = args.size() > 2 ? atoi(args[2].c_str()) : 20;
    ReplyActionResult(caller, fromChat, BurnPlayer(target, caller, seconds));
}

static void HandleHealth(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 3, "UsageValue", "@admin/hp");
    if (target != -1)
        ReplyActionResult(caller, fromChat, SetPlayerHealth(target, caller, atoi(args[2].c_str())));
}

static void HandleArmor(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 3, "UsageValue", "@admin/armor");
    if (target != -1)
        ReplyActionResult(caller, fromChat, SetPlayerArmor(target, caller, atoi(args[2].c_str())));
}

static void HandleGravity(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 3, "UsageValue", "@admin/gravity");
    if (target != -1)
        ReplyActionResult(caller, fromChat, SetPlayerGravity(target, caller, (float)atof(args[2].c_str())));
}

static void HandleSpeed(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 3, "UsageValue", "@admin/speed");
    if (target != -1)
        ReplyActionResult(caller, fromChat, SetPlayerSpeed(target, caller, (float)atof(args[2].c_str())));
}

static void HandleGod(int caller, bool fromChat, const std::vector<std::string>& args)
{
    int target = TargetForAction(caller, fromChat, args, 2, "UsageTarget", "@admin/god");
    if (target != -1)
        ReplyActionResult(caller, fromChat, ToggleGodMode(target, caller));
}

static void HandleRcon(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/rcon"))
        return;
    if (args.size() < 2)
    {
        ReplyUsage(caller, fromChat, "UsageRcon", args);
        return;
    }
    RunServerCommand(caller, JoinFrom(args, 1));
}

static void HandlePlayerVote(int caller, bool fromChat, const std::vector<std::string>& args, int type)
{
    int target = TargetForAction(caller, fromChat, args, 2, "UsageTarget", "@admin/vote");
    if (target != -1 && !StartPlayerVote(caller, type, target) && !IsValidSlot(caller))
        ReplyTo(caller, fromChat, Text("VoteAlreadyRunning"));
}

static void HandleVoteKick(int caller, bool fromChat, const std::vector<std::string>& args)
{
    HandlePlayerVote(caller, fromChat, args, VOTE_KICK);
}

static void HandleVoteBan(int caller, bool fromChat, const std::vector<std::string>& args)
{
    HandlePlayerVote(caller, fromChat, args, VOTE_BAN);
}

static void HandleVoteMute(int caller, bool fromChat, const std::vector<std::string>& args)
{
    HandlePlayerVote(caller, fromChat, args, VOTE_MUTE);
}

static void HandleVoteMap(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (!CheckAccess(caller, fromChat, "@admin/vote"))
        return;
    if (args.size() < 2)
    {
        ReplyUsage(caller, fromChat, "UsageMap", args);
        return;
    }
    if (!StartMapVote(caller, args[1]) && !IsValidSlot(caller))
        ReplyTo(caller, fromChat, Text("VoteAlreadyRunning"));
}

static const CommandEntry g_Commands[] = {
    { "ban", HandlePunish },
    { "mute", HandlePunish },
    { "gag", HandlePunish },
    { "silence", HandlePunish },
    { "unban", HandleUnpunish },
    { "unmute", HandleUnpunish },
    { "ungag", HandleUnpunish },
    { "unsilence", HandleUnpunish },
    { "banip", HandleBanIp },
    { "kick", HandleKick },
    { "add_admin", HandleAddAdmin },
    { "remove_admin", HandleRemoveAdmin },
    { "add_group", HandleAddGroup },
    { "remove_group", HandleRemoveGroup },
    { "reload", HandleReloadConfig },
    { "reload_admin", HandleReloadAdmin },
    { "reload_punish", HandleReloadPunish },
    { "sync", HandleSync },
    { "import", HandleImport },
    { "slay", HandleSlay },
    { "slap", HandleSlap },
    { "rename", HandleRename },
    { "team", HandleTeam },
    { "noclip", HandleNoclip },
    { "freeze", HandleFreeze },
    { "respawn", HandleRespawn },
    { "who", HandleWho },
    { "map", HandleMap },
    { "restart", HandleRestart },
    { "swapteams", HandleSwapTeams },
    { "burn", HandleBurn },
    { "hp", HandleHealth },
    { "armor", HandleArmor },
    { "gravity", HandleGravity },
    { "speed", HandleSpeed },
    { "god", HandleGod },
    { "rcon", HandleRcon },
    { "votekick", HandleVoteKick },
    { "voteban", HandleVoteBan },
    { "votemute", HandleVoteMute },
    { "votemap", HandleVoteMap },
    { "timebomb", HandleTimeBomb },
    { "firebomb", HandleFireBomb },
    { "freezebomb", HandleFreezeBomb }
};

static std::string BareName(const std::string& name)
{
    std::string bare = Lowercase(name);
    if (bare.compare(0, 3, "vd_") == 0)
        return bare.substr(3);
    if (!bare.empty() && (bare[0] == '!' || bare[0] == '/'))
        return bare.substr(1);
    return bare;
}

static void Dispatch(int caller, bool fromChat, const std::vector<std::string>& args)
{
    if (args.empty())
        return;

    std::string name = BareName(args[0]);
    for (size_t i = 0; i < sizeof(g_Commands) / sizeof(g_Commands[0]); i++)
    {
        if (name == g_Commands[i].name)
        {
            g_Commands[i].handler(caller, fromChat, args);
            return;
        }
    }
}

static void OnConsoleCommand(const CCommandContext& context, const CCommand& command)
{
    Dispatch(context.GetPlayerSlot().Get(), false, SplitArguments(command));
}

CON_COMMAND_EXTERN(vd_ban, OnConsoleCommand, "Verdict: ban a player");
CON_COMMAND_EXTERN(vd_mute, OnConsoleCommand, "Verdict: mute a player");
CON_COMMAND_EXTERN(vd_gag, OnConsoleCommand, "Verdict: gag a player");
CON_COMMAND_EXTERN(vd_silence, OnConsoleCommand, "Verdict: silence a player");
CON_COMMAND_EXTERN(vd_unban, OnConsoleCommand, "Verdict: unban a player");
CON_COMMAND_EXTERN(vd_unmute, OnConsoleCommand, "Verdict: unmute a player");
CON_COMMAND_EXTERN(vd_ungag, OnConsoleCommand, "Verdict: ungag a player");
CON_COMMAND_EXTERN(vd_unsilence, OnConsoleCommand, "Verdict: unsilence a player");
CON_COMMAND_EXTERN(vd_banip, OnConsoleCommand, "Verdict: ban an ip address");
CON_COMMAND_EXTERN(vd_kick, OnConsoleCommand, "Verdict: kick a player");
CON_COMMAND_EXTERN(vd_add_admin, OnConsoleCommand, "Verdict: add an admin");
CON_COMMAND_EXTERN(vd_remove_admin, OnConsoleCommand, "Verdict: remove an admin");
CON_COMMAND_EXTERN(vd_add_group, OnConsoleCommand, "Verdict: add a group");
CON_COMMAND_EXTERN(vd_remove_group, OnConsoleCommand, "Verdict: remove a group");
CON_COMMAND_EXTERN(vd_reload, OnConsoleCommand, "Verdict: reload config and phrases");
CON_COMMAND_EXTERN(vd_reload_admin, OnConsoleCommand, "Verdict: reload admin rights of a player");
CON_COMMAND_EXTERN(vd_reload_punish, OnConsoleCommand, "Verdict: reload punishments of a player");
CON_COMMAND_EXTERN(vd_sync, OnConsoleCommand, "Verdict: reload admins and punishments");
CON_COMMAND_EXTERN(vd_import, OnConsoleCommand, "Verdict: import data from cs2-admin_system tables");
CON_COMMAND_EXTERN(vd_slay, OnConsoleCommand, "Verdict: kill a player");
CON_COMMAND_EXTERN(vd_slap, OnConsoleCommand, "Verdict: slap a player");
CON_COMMAND_EXTERN(vd_rename, OnConsoleCommand, "Verdict: rename a player");
CON_COMMAND_EXTERN(vd_team, OnConsoleCommand, "Verdict: move a player to a team");
CON_COMMAND_EXTERN(vd_noclip, OnConsoleCommand, "Verdict: toggle noclip");
CON_COMMAND_EXTERN(vd_freeze, OnConsoleCommand, "Verdict: toggle freeze");
CON_COMMAND_EXTERN(vd_respawn, OnConsoleCommand, "Verdict: respawn a player");
CON_COMMAND_EXTERN(vd_who, OnConsoleCommand, "Verdict: show player info");
CON_COMMAND_EXTERN(vd_map, OnConsoleCommand, "Verdict: change the map");
CON_COMMAND_EXTERN(vd_restart, OnConsoleCommand, "Verdict: restart the game");
CON_COMMAND_EXTERN(vd_swapteams, OnConsoleCommand, "Verdict: swap the teams");
CON_COMMAND_EXTERN(vd_burn, OnConsoleCommand, "Verdict: set a player on fire");
CON_COMMAND_EXTERN(vd_hp, OnConsoleCommand, "Verdict: set player health");
CON_COMMAND_EXTERN(vd_armor, OnConsoleCommand, "Verdict: set player armor");
CON_COMMAND_EXTERN(vd_gravity, OnConsoleCommand, "Verdict: set player gravity");
CON_COMMAND_EXTERN(vd_speed, OnConsoleCommand, "Verdict: set player speed");
CON_COMMAND_EXTERN(vd_god, OnConsoleCommand, "Verdict: toggle god mode");
CON_COMMAND_EXTERN(vd_rcon, OnConsoleCommand, "Verdict: run a server command");
CON_COMMAND_EXTERN(vd_votekick, OnConsoleCommand, "Verdict: start a kick vote");
CON_COMMAND_EXTERN(vd_voteban, OnConsoleCommand, "Verdict: start a ban vote");
CON_COMMAND_EXTERN(vd_votemute, OnConsoleCommand, "Verdict: start a mute vote");
CON_COMMAND_EXTERN(vd_votemap, OnConsoleCommand, "Verdict: start a map vote");
CON_COMMAND_EXTERN(vd_timebomb, OnConsoleCommand, "Verdict: toggle a time bomb on a player");
CON_COMMAND_EXTERN(vd_firebomb, OnConsoleCommand, "Verdict: toggle a fire bomb on a player");
CON_COMMAND_EXTERN(vd_freezebomb, OnConsoleCommand, "Verdict: toggle a freeze bomb on a player");

static bool OnChatCommand(int slot, const char* content)
{
    CCommand command;
    command.Tokenize(content);
    Dispatch(slot, true, SplitArguments(command));
    return true;
}

void RegisterChatCommands()
{
    std::vector<std::string> chatNames;
    const char* names[] = { "ban", "mute", "gag", "silence", "unban", "unmute", "ungag", "unsilence", "banip", "kick",
        "add_admin", "remove_admin", "add_group", "remove_group", "slay", "slap", "rename", "team", "noclip", "freeze",
        "respawn", "who", "map", "restart", "swapteams", "burn", "hp", "armor", "gravity", "speed", "god",
        "rcon", "votekick", "voteban", "votemute", "votemap", "timebomb", "firebomb", "freezebomb" };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        chatNames.push_back(std::string("!") + names[i]);

    g_pUtils->RegCommand(g_PLID, std::vector<std::string>(), chatNames, OnChatCommand);
    g_pUtils->RegCommand(g_PLID, { "vd_admin" }, { "!admin" }, [](int slot, const char*) {
        return ShowAdminMenu(slot);
    });
}
