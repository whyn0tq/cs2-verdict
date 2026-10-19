#include "panel_link.h"
#include "admins.h"
#include "clients.h"
#include "entities.h"
#include "phrases.h"
#include "player_actions.h"
#include "plugin.h"
#include "punishments.h"
#include "steam.h"
#include "storage.h"
#include "common.h"

extern CEntitySystem* g_pEntitySystem;

struct QueuedCommand
{
    int id;
    std::string name;
    std::string args;
};

static int g_JoinedAt[MAX_SLOTS];
static int g_LastSnapshot = 0;
static bool g_SnapshotBusy = false;
static bool g_QueueBusy = false;

static const char* g_PanelTables[] = {
    "CREATE TABLE IF NOT EXISTS `vd_live_servers` ("
    "`sid` int NOT NULL,"
    "`version` varchar(32) NULL,"
    "`score_t` int NOT NULL DEFAULT 0,"
    "`score_ct` int NOT NULL DEFAULT 0,"
    "`updated` int NOT NULL DEFAULT 0,"
    "PRIMARY KEY (`sid`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `vd_live_players` ("
    "`sid` int NOT NULL,"
    "`slot` int NOT NULL,"
    "`steamid` varchar(20) NULL,"
    "`name` varchar(128) NOT NULL DEFAULT '',"
    "`bot` tinyint NOT NULL DEFAULT 0,"
    "`team` tinyint NOT NULL DEFAULT 0,"
    "`alive` tinyint NOT NULL DEFAULT 0,"
    "`health` int NOT NULL DEFAULT 0,"
    "`kills` int NOT NULL DEFAULT 0,"
    "`deaths` int NOT NULL DEFAULT 0,"
    "`assists` int NOT NULL DEFAULT 0,"
    "`score` int NOT NULL DEFAULT 0,"
    "`ping` int NOT NULL DEFAULT 0,"
    "`ip` varchar(45) NULL,"
    "`connected` int NOT NULL DEFAULT 0,"
    "PRIMARY KEY (`sid`, `slot`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `vd_feed` ("
    "`id` bigint NOT NULL AUTO_INCREMENT,"
    "`sid` int NOT NULL,"
    "`kind` varchar(16) NOT NULL,"
    "`steamid` varchar(20) NULL,"
    "`name` varchar(128) NULL,"
    "`text` varchar(512) NULL,"
    "`meta` varchar(255) NULL,"
    "`created` int NOT NULL,"
    "PRIMARY KEY (`id`),"
    "KEY `server_feed` (`sid`, `id`),"
    "KEY `created` (`created`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `vd_queue` ("
    "`id` int NOT NULL AUTO_INCREMENT,"
    "`sid` int NOT NULL,"
    "`command` varchar(32) NOT NULL,"
    "`args` text NOT NULL,"
    "`status` tinyint NOT NULL DEFAULT 0,"
    "`result` varchar(255) NULL,"
    "`created` int NOT NULL,"
    "PRIMARY KEY (`id`),"
    "KEY `pending` (`sid`, `status`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
};

void CreatePanelTables()
{
    for (size_t i = 0; i < sizeof(g_PanelTables) / sizeof(g_PanelTables[0]); i++)
        RunQuery(g_PanelTables[i]);
}

static bool LinkEnabled()
{
    return IsDatabaseReady() && ServerId() > 0;
}

static std::string ServerIdText()
{
    return std::to_string(ServerId());
}

static const char* TeamCode(int team)
{
    if (team == TEAM_T)
        return "t";
    if (team == TEAM_CT)
        return "ct";
    if (team == TEAM_SPECTATOR)
        return "spec";
    return "none";
}

static std::string JsonEscape(const std::string& text)
{
    std::string result;
    for (size_t i = 0; i < text.length(); i++)
    {
        unsigned char c = (unsigned char)text[i];
        if (c == '"' || c == '\\')
        {
            result += '\\';
            result += (char)c;
        }
        else if (c < 0x20)
        {
            char code[8];
            snprintf(code, sizeof(code), "\\u%04x", c);
            result += code;
        }
        else
        {
            result += (char)c;
        }
    }
    return result;
}

static void AppendUtf8(std::string& output, unsigned int code)
{
    if (code < 0x80)
    {
        output += (char)code;
    }
    else if (code < 0x800)
    {
        output += (char)(0xC0 | (code >> 6));
        output += (char)(0x80 | (code & 0x3F));
    }
    else if (code < 0x10000)
    {
        output += (char)(0xE0 | (code >> 12));
        output += (char)(0x80 | ((code >> 6) & 0x3F));
        output += (char)(0x80 | (code & 0x3F));
    }
    else
    {
        output += (char)(0xF0 | (code >> 18));
        output += (char)(0x80 | ((code >> 12) & 0x3F));
        output += (char)(0x80 | ((code >> 6) & 0x3F));
        output += (char)(0x80 | (code & 0x3F));
    }
}

static unsigned int ReadHex4(const std::string& text, size_t position)
{
    if (position + 4 > text.length())
        return 0;
    return (unsigned int)strtoul(text.substr(position, 4).c_str(), nullptr, 16);
}

static std::string JsonField(const std::string& json, const char* name)
{
    std::string key = std::string("\"") + name + "\"";
    size_t position = json.find(key);
    if (position == std::string::npos)
        return "";
    position = json.find(':', position + key.length());
    if (position == std::string::npos)
        return "";
    position = json.find('"', position);
    if (position == std::string::npos)
        return "";

    std::string value;
    for (size_t i = position + 1; i < json.length(); i++)
    {
        char c = json[i];
        if (c == '"')
            break;
        if (c != '\\' || i + 1 >= json.length())
        {
            value += c;
            continue;
        }

        char escaped = json[++i];
        if (escaped == 'n')
            value += '\n';
        else if (escaped == 't')
            value += '\t';
        else if (escaped == 'r' || escaped == 'b' || escaped == 'f')
            value += ' ';
        else if (escaped == 'u')
        {
            unsigned int code = ReadHex4(json, i + 1);
            i += 4;
            if (code >= 0xD800 && code <= 0xDBFF && i + 6 < json.length() && json[i + 1] == '\\' && json[i + 2] == 'u')
            {
                unsigned int low = ReadHex4(json, i + 3);
                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                i += 6;
            }
            AppendUtf8(value, code);
        }
        else
            value += escaped;
    }
    return value;
}

static std::string NullableQuote(const std::string& text)
{
    if (text.empty())
        return "NULL";
    return Quote(text);
}

static void AddFeedEvent(const char* kind, uint64 steamId, const std::string& name, const std::string& text, const std::string& meta)
{
    if (!LinkEnabled())
        return;

    std::string steamText = steamId != 0 ? SteamIdToText(steamId) : "";
    RunQuery("INSERT INTO `vd_feed` (`sid`, `kind`, `steamid`, `name`, `text`, `meta`, `created`) VALUES (" +
        ServerIdText() + ", " + Quote(kind) + ", " + NullableQuote(steamText) + ", " + NullableQuote(CleanText(name, 120)) + ", " +
        NullableQuote(CleanText(text, 500)) + ", " + NullableQuote(meta) + ", UNIX_TIMESTAMP())");
}

void PanelPlayerJoined(int slot)
{
    if (!IsValidSlot(slot))
        return;
    g_JoinedAt[slot] = Now();
    AddFeedEvent("join", ClientSteamId(slot), ClientName(slot), "", "");
}

void PanelPlayerLeft(int slot)
{
    if (!IsValidSlot(slot))
        return;
    if (IsRealClient(slot))
        AddFeedEvent("leave", ClientSteamId(slot), ClientName(slot), "", "");
    g_JoinedAt[slot] = 0;
}

void PanelChatMessage(int slot, const std::string& text, bool teamOnly, bool blocked)
{
    if (!IsRealClient(slot) || text.empty() || text[0] == '/')
        return;

    int team = TeamOf(slot);
    bool playing = team == TEAM_T || team == TEAM_CT;
    std::string meta = std::string("{\"team\":\"") + TeamCode(team) + "\"";
    if (teamOnly)
        meta += ",\"teamOnly\":true";
    if (playing && !IsAlive(slot))
        meta += ",\"dead\":true";
    if (blocked)
        meta += ",\"blocked\":true";
    meta += "}";

    AddFeedEvent("chat", ClientSteamId(slot), ClientName(slot), text, meta);
}

static int TeamScore(int team)
{
    if (!g_pEntitySystem)
        return 0;

    for (int index = 0; index < 1024; index++)
    {
        CEntityInstance* entity = g_pEntitySystem->GetEntityInstance(CEntityIndex(index));
        if (!entity || !entity->GetClassname() || strcmp(entity->GetClassname(), "cs_team_manager") != 0)
            continue;
        if (EntityTeam(entity) == team)
            return TeamScoreOf(entity);
    }
    return 0;
}

static std::string PlayerRow(int slot)
{
    bool bot = g_pPlayers->IsFakeClient(slot);
    uint64 steamId = bot ? 0 : ClientSteamId(slot);
    if (!bot && steamId == 0)
        return "";
    if (g_JoinedAt[slot] == 0)
        g_JoinedAt[slot] = Now();

    const char* rawName = g_pPlayers->GetPlayerName(slot);
    std::string name = CleanName(rawName ? rawName : "");

    std::string row = "(" + ServerIdText() + ", " + std::to_string(slot) + ", " +
        (steamId != 0 ? Quote(SteamIdToText(steamId)) : std::string("NULL")) + ", " + Quote(name) + ", " +
        std::to_string(bot ? 1 : 0) + ", " + std::to_string(TeamOf(slot)) + ", " + std::to_string(IsAlive(slot) ? 1 : 0) + ", " +
        std::to_string(HealthOf(slot)) + ", " + std::to_string(MatchStatOf(slot, "m_iKills")) + ", " +
        std::to_string(MatchStatOf(slot, "m_iDeaths")) + ", " + std::to_string(MatchStatOf(slot, "m_iAssists")) + ", " +
        std::to_string(ControllerIntOf(slot, "m_iScore")) + ", " + std::to_string(ControllerIntOf(slot, "m_iPing")) + ", " +
        NullableQuote(bot ? "" : ClientIp(slot)) + ", " + std::to_string(bot ? 0 : g_JoinedAt[slot]) + ")";
    return row;
}

static void WriteSnapshot()
{
    if (g_SnapshotBusy || !g_pConnection)
        return;

    std::string rows;
    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (!g_pPlayers->IsConnected(slot))
        {
            g_JoinedAt[slot] = 0;
            continue;
        }
        std::string row = PlayerRow(slot);
        if (row.empty())
            continue;
        if (!rows.empty())
            rows += ", ";
        rows += row;
    }

    Transaction transaction;
    transaction.queries.push_back("DELETE FROM `vd_live_players` WHERE `sid` = " + ServerIdText());
    if (!rows.empty())
    {
        transaction.queries.push_back("INSERT INTO `vd_live_players` (`sid`, `slot`, `steamid`, `name`, `bot`, `team`, `alive`, `health`, "
            "`kills`, `deaths`, `assists`, `score`, `ping`, `ip`, `connected`) VALUES " + rows);
    }
    transaction.queries.push_back("REPLACE INTO `vd_live_servers` (`sid`, `version`, `score_t`, `score_ct`, `updated`) VALUES (" +
        ServerIdText() + ", " + Quote(g_PLAPI->GetVersion()) + ", " + std::to_string(TeamScore(TEAM_T)) + ", " +
        std::to_string(TeamScore(TEAM_CT)) + ", UNIX_TIMESTAMP())");

    g_SnapshotBusy = true;
    g_pConnection->ExecuteTransaction(transaction, [](std::vector<ISQLQuery*>) {
        g_SnapshotBusy = false;
    }, [](std::string error, int code) {
        g_SnapshotBusy = false;
        g_pUtils->ErrorLog("[Verdict] Live status was not saved (%d): %s", code, error.c_str());
    });
}

static int TeamFromCode(const std::string& code)
{
    if (code == "t")
        return TEAM_T;
    if (code == "ct")
        return TEAM_CT;
    if (code == "spec")
        return TEAM_SPECTATOR;
    return TEAM_NONE;
}

static int TargetSlot(const std::string& args)
{
    uint64 steamId = 0;
    if (!ParseSteamId(JsonField(args, "steamid"), steamId))
        return -1;
    return FindClientBySteamId(steamId);
}

static std::string PanelSay(const std::string& args)
{
    std::string text = CleanText(JsonField(args, "text"), 200);
    std::string admin = CleanText(JsonField(args, "admin"), 64);
    if (text.empty())
        return "message_empty";

    std::string target = JsonField(args, "steamid");
    std::string meta = "{\"admin\":\"" + JsonEscape(admin) + "\"}";
    if (target.empty())
    {
        std::string message = Text("PanelMessage");
        Replace(message, "{admin}", admin);
        Replace(message, "{text}", text);
        ChatToAll(message);
        AddFeedEvent("panel_chat", 0, "", text, meta);
        return "";
    }

    int slot = TargetSlot(args);
    if (slot == -1)
        return "player_not_found";

    std::string message = Text("PanelPrivateMessage");
    Replace(message, "{admin}", admin);
    Replace(message, "{text}", text);
    ChatTo(slot, message);
    AddFeedEvent("panel_chat", ClientSteamId(slot), ClientName(slot), text, meta);
    return "";
}

static std::string RunPanelCommand(const QueuedCommand& command)
{
    if (command.name == "say")
        return PanelSay(command.args);

    if (command.name == "reload_admins")
    {
        for (int slot = 0; slot < MAX_SLOTS; slot++)
        {
            if (IsRealClient(slot))
                LoadAdmin(slot, ClientSteamId(slot));
        }
        return "";
    }

    int slot = TargetSlot(command.args);

    if (command.name == "sync")
    {
        if (slot != -1)
            SyncPlayer(slot);
        return "";
    }

    if (slot == -1)
        return "player_not_found";

    if (command.name == "kick")
    {
        KickWithAnnounce(slot, CONSOLE_SLOT, JsonField(command.args, "reason"));
        return "";
    }

    if (command.name == "slay")
        return SlayPlayer(slot, CONSOLE_SLOT) ? "" : "player_dead";

    if (command.name == "team")
    {
        int team = TeamFromCode(JsonField(command.args, "team"));
        if (team == TEAM_NONE)
            return "invalid_team";
        if (TeamOf(slot) == team)
            return "already_in_team";
        return MovePlayerToTeam(slot, CONSOLE_SLOT, team) ? "" : "player_not_found";
    }

    return "unknown_command";
}

static void FinishCommand(int id, const std::string& error)
{
    RunQuery("UPDATE `vd_queue` SET `status` = " + std::string(error.empty() ? "1" : "2") + ", `result` = " +
        Quote(error.empty() ? "ok" : error) + " WHERE `id` = " + std::to_string(id));
}

static void PollQueue()
{
    if (g_QueueBusy)
        return;

    g_QueueBusy = true;
    std::string query = "SELECT `id`, `command`, `args` FROM `vd_queue` WHERE `sid` = " + ServerIdText() +
        " AND `status` = 0 AND `created` > UNIX_TIMESTAMP() - 30 ORDER BY `id` LIMIT 10";

    RunQuery(query, [](ISQLQuery* query) {
        g_QueueBusy = false;
        ISQLResult* result = ResultOf(query);
        if (!result)
            return;

        std::vector<QueuedCommand> commands;
        while (result->FetchRow())
        {
            QueuedCommand command;
            command.id = result->GetInt(0);
            command.name = ReadString(result, 1);
            command.args = ReadString(result, 2);
            commands.push_back(command);
        }

        for (size_t i = 0; i < commands.size(); i++)
            FinishCommand(commands[i].id, RunPanelCommand(commands[i]));
    });
}

void PanelLinkTick()
{
    if (!LinkEnabled())
        return;

    PollQueue();

    if (Now() - g_LastSnapshot >= 5)
    {
        g_LastSnapshot = Now();
        WriteSnapshot();
    }
}
