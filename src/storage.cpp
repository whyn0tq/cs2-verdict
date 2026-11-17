#include "storage.h"
#include "panel_link.h"
#include "settings.h"
#include "phrases.h"
#include "common.h"
#include "filesystem/ifilesystem.h"

static bool g_DatabaseReady = false;
static int g_ServerId = 0;

static const char* g_Schema[] = {
    "CREATE TABLE IF NOT EXISTS `{p}admins` ("
    "`aid` int(6) NOT NULL AUTO_INCREMENT,"
    "`user` varchar(64) NOT NULL,"
    "`authid` varchar(64) NOT NULL DEFAULT '',"
    "`password` varchar(128) NOT NULL DEFAULT '',"
    "`gid` int(6) NOT NULL DEFAULT -1,"
    "`email` varchar(128) NOT NULL DEFAULT '',"
    "`validate` varchar(128) NULL DEFAULT NULL,"
    "`extraflags` int(10) NOT NULL DEFAULT 0,"
    "`immunity` int(10) NOT NULL DEFAULT 0,"
    "`srv_group` varchar(128) DEFAULT NULL,"
    "`srv_flags` varchar(64) DEFAULT NULL,"
    "`srv_password` varchar(128) DEFAULT NULL,"
    "`lastvisit` int(11) NULL,"
    "`expired` int(11) NULL DEFAULT 0,"
    "`skype` varchar(128) NULL,"
    "`comment` varchar(128) NULL,"
    "`vk` varchar(128) NULL,"
    "`support` int(6) NULL DEFAULT 0,"
    "PRIMARY KEY (`aid`),"
    "UNIQUE KEY `user` (`user`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `{p}admins_servers_groups` ("
    "`admin_id` int(10) NOT NULL,"
    "`group_id` int(10) NOT NULL,"
    "`srv_group_id` int(10) NOT NULL,"
    "`server_id` int(10) NOT NULL,"
    "KEY `admin_id` (`admin_id`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `{p}banlog` ("
    "`sid` int(6) NOT NULL,"
    "`time` int(11) NOT NULL,"
    "`name` varchar(128) NOT NULL,"
    "`bid` int(6) NOT NULL,"
    "PRIMARY KEY (`sid`, `time`, `bid`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `{p}bans` ("
    "`bid` int(6) NOT NULL AUTO_INCREMENT,"
    "`ip` varchar(32) DEFAULT NULL,"
    "`authid` varchar(64) NOT NULL DEFAULT '',"
    "`name` varchar(128) NOT NULL DEFAULT 'unnamed',"
    "`created` int(11) NOT NULL DEFAULT 0,"
    "`ends` int(11) NOT NULL DEFAULT 0,"
    "`length` int(10) NOT NULL DEFAULT 0,"
    "`reason` text NOT NULL,"
    "`aid` int(6) NOT NULL DEFAULT 0,"
    "`adminIp` varchar(32) NOT NULL DEFAULT '',"
    "`sid` int(6) NOT NULL DEFAULT 0,"
    "`country` varchar(4) DEFAULT NULL,"
    "`RemovedBy` int(8) NULL,"
    "`RemoveType` varchar(3) NULL,"
    "`RemovedOn` int(10) NULL,"
    "`type` tinyint NOT NULL DEFAULT 0,"
    "`ureason` text,"
    "PRIMARY KEY (`bid`),"
    "KEY `sid` (`sid`),"
    "KEY `created` (`created`),"
    "KEY `steam_lookup` (`authid`, `type`),"
    "KEY `ip_lookup` (`ip`, `type`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `{p}comms` ("
    "`bid` int(6) NOT NULL AUTO_INCREMENT,"
    "`authid` varchar(64) NOT NULL,"
    "`name` varchar(128) NOT NULL DEFAULT 'unnamed',"
    "`created` int(11) NOT NULL DEFAULT 0,"
    "`ends` int(11) NOT NULL DEFAULT 0,"
    "`length` int(10) NOT NULL DEFAULT 0,"
    "`reason` text NOT NULL,"
    "`aid` int(6) NOT NULL DEFAULT 0,"
    "`adminIp` varchar(32) NOT NULL DEFAULT '',"
    "`sid` int(6) NOT NULL DEFAULT 0,"
    "`RemovedBy` int(8) DEFAULT NULL,"
    "`RemoveType` varchar(3) DEFAULT NULL,"
    "`RemovedOn` int(11) DEFAULT NULL,"
    "`type` tinyint(4) NOT NULL DEFAULT 0,"
    "`ureason` text,"
    "PRIMARY KEY (`bid`),"
    "KEY `sid` (`sid`),"
    "KEY `type` (`type`),"
    "KEY `RemoveType` (`RemoveType`),"
    "KEY `authid` (`authid`),"
    "KEY `created` (`created`),"
    "KEY `aid` (`aid`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `{p}servers` ("
    "`sid` int(6) NOT NULL AUTO_INCREMENT,"
    "`priority` int(11) NOT NULL DEFAULT 0,"
    "`ip` varchar(64) NOT NULL,"
    "`port` int(5) NOT NULL,"
    "`rcon` varchar(256) NOT NULL DEFAULT '',"
    "`modid` int(10) NOT NULL DEFAULT 0,"
    "`enabled` tinyint NOT NULL DEFAULT 1,"
    "PRIMARY KEY (`sid`),"
    "UNIQUE KEY `ip` (`ip`, `port`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `{p}servers_groups` ("
    "`server_id` int(10) NOT NULL,"
    "`group_id` int(10) NOT NULL,"
    "PRIMARY KEY (`server_id`, `group_id`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `{p}srvgroups` ("
    "`id` int(10) unsigned NOT NULL AUTO_INCREMENT,"
    "`flags` varchar(30) NOT NULL,"
    "`immunity` int(10) unsigned NOT NULL DEFAULT 0,"
    "`name` varchar(120) NOT NULL,"
    "`groups_immune` varchar(255) NOT NULL DEFAULT '',"
    "`maxbantime` int NOT NULL DEFAULT -1,"
    "`maxmutetime` int NOT NULL DEFAULT -1,"
    "PRIMARY KEY (`id`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

    "CREATE TABLE IF NOT EXISTS `{p}settings` ("
    "`setting` varchar(128) NOT NULL,"
    "`value` text NOT NULL,"
    "UNIQUE KEY `setting` (`setting`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
};

struct ColumnPatch
{
    const char* table;
    const char* column;
    const char* definition;
};

static const ColumnPatch g_ColumnPatches[] = {
    { "admins", "expired", "int(11) NULL DEFAULT 0" },
    { "admins", "comment", "varchar(128) NULL" },
    { "srvgroups", "maxbantime", "int NOT NULL DEFAULT -1" },
    { "srvgroups", "maxmutetime", "int NOT NULL DEFAULT -1" }
};

std::string TablePrefix()
{
    std::string prefix = g_Settings.tablePrefix;
    if (!prefix.empty() && prefix[prefix.length() - 1] != '_')
        prefix += "_";
    return prefix;
}

std::string Table(const char* name)
{
    return "`" + TablePrefix() + name + "`";
}

bool IsDatabaseReady()
{
    return g_DatabaseReady;
}

int ServerId()
{
    return g_ServerId;
}

std::string PunishServerFilter(const char* column)
{
    std::string id = std::to_string(g_ServerId);
    if (g_Settings.punishScope == 1)
        return std::string(" AND ") + column + " = " + id;
    if (g_Settings.punishScope == 2)
        return std::string(" AND (") + column + " = " + id + " OR " + column + " = 0)";
    return "";
}

std::string AdminServerFilter(const char* adminColumn)
{
    if (g_ServerId <= 0)
        return "";

    std::string id = std::to_string(g_ServerId);
    return std::string(" AND ") + adminColumn + " IN (SELECT `admin_id` FROM " + Table("admins_servers_groups") +
        " WHERE `server_id` = " + id + " OR `server_id` = -1 OR `srv_group_id` IN (SELECT `group_id` FROM " +
        Table("servers_groups") + " WHERE `server_id` = " + id + "))";
}

static void FinishLoading()
{
    g_DatabaseReady = true;
    META_CONPRINTF("[Verdict] Database ready, server id %d\n", g_ServerId);
    OnDatabaseReady();
}

static void LogSetupError(const std::string& error)
{
    g_pUtils->ErrorLog("[Verdict] Database setup failed: %s", error.c_str());
}

static void ResolveServerId()
{
    if (g_Settings.serverId >= 0)
    {
        g_ServerId = g_Settings.serverId;
        FinishLoading();
        return;
    }

    std::string address = g_Settings.serverAddress;
    size_t colon = address.find(':');
    if (colon == std::string::npos)
    {
        g_ServerId = 0;
        g_pUtils->ErrorLog("[Verdict] server_id is not set and server_address is empty, admins from all servers will be loaded");
        FinishLoading();
        return;
    }

    std::string ip = address.substr(0, colon);
    int port = atoi(address.substr(colon + 1).c_str());
    std::string query = "SELECT `sid` FROM " + Table("servers") + " WHERE `ip` = " + Quote(ip) + " AND `port` = " + std::to_string(port) + " LIMIT 1";

    RunQuery(query, [](sql::Result& result) {
        if (!result.Ok())
        {
            LogSetupError(result.Error());
            return;
        }
        if (result.Next())
            g_ServerId = result.Int(0);
        else
            g_pUtils->ErrorLog("[Verdict] Server %s is not registered in the panel", g_Settings.serverAddress.c_str());
        FinishLoading();
    });
}

static void PatchColumns(size_t index, std::string alterError = {})
{
    if (index == sizeof(g_ColumnPatches) / sizeof(g_ColumnPatches[0]))
    {
        ResolveServerId();
        return;
    }

    const auto& patch = g_ColumnPatches[index];
    const std::string table = TablePrefix() + patch.table;
    const std::string column = patch.column;
    const std::string query = "SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = " +
        Quote(table) + " AND COLUMN_NAME = " + Quote(column);
    const std::string alter = "ALTER TABLE `" + table + "` ADD `" + column + "` " + patch.definition;

    RunQuery(query, [index, alter, alterError = std::move(alterError)](sql::Result& result) {
        if (!result.Ok() || !result.Next())
        {
            LogSetupError(result.Ok() ? "Missing column count" : result.Error());
            return;
        }
        if (result.Int(0) > 0)
        {
            PatchColumns(index + 1);
            return;
        }
        if (!alterError.empty())
        {
            LogSetupError(alterError);
            return;
        }
        RunQuery(alter, [index](sql::Result& change) {
            if (change.Ok())
                PatchColumns(index + 1);
            else
                PatchColumns(index, change.Error());
        });
    });
}

static void CreateTables(std::vector<std::string> queries, size_t index)
{
    if (index == queries.size())
    {
        PatchColumns(0);
        return;
    }

    std::string query = std::move(queries[index]);
    RunQuery(std::move(query), [queries = std::move(queries), index](sql::Result& result) mutable {
        if (result.Ok())
            CreateTables(std::move(queries), index + 1);
        else
            LogSetupError(result.Error());
    });
}

static void CreateTables()
{
    std::vector<std::string> queries;
    for (const char* schema : g_Schema)
    {
        std::string query = schema;
        Replace(query, "{p}", TablePrefix());
        queries.push_back(std::move(query));
    }
    AddPanelTables(queries);
    CreateTables(std::move(queries), 0);
}

static KeyValues* FindDatabaseSection(KeyValues* databases)
{
    KeyValues* section = databases->FindKey("verdict", false);
    if (!section)
        section = databases->FindKey("admin_system", false);
    if (!section)
        section = databases->FindKey("materialadmin", false);
    return section;
}

void ConnectDatabase(sql::IApi& api)
{
    DisconnectDatabase();

    KeyValues* databases = new KeyValues("Databases");
    if (!databases->LoadFromFile(g_pFullFileSystem, "addons/configs/databases.cfg"))
    {
        g_pUtils->ErrorLog("[Verdict] Failed to load addons/configs/databases.cfg");
        delete databases;
        return;
    }

    KeyValues* section = FindDatabaseSection(databases);
    if (!section)
    {
        g_pUtils->ErrorLog("[Verdict] No \"verdict\" section in addons/configs/databases.cfg");
        delete databases;
        return;
    }

    sql::Config config;
    config.kind = sql::Kind::MySQL;
    config.host = section->GetString("host", "127.0.0.1");
    config.user = section->GetString("user", "");
    config.pass = section->GetString("pass", "");
    config.name = section->GetString("database", "");
    config.port = section->GetInt("port", 3306);
    delete databases;

    if (!OpenDatabase(api, config, [](bool ok, const std::string& error) {
        if (ok)
            CreateTables();
        else
            g_pUtils->ErrorLog("[Verdict] Database connection failed: %s", error.c_str());
    }))
        g_pUtils->ErrorLog("[Verdict] Failed to open a MySQL connection through cs2-DBMP");
}

void DisconnectDatabase(bool providerLoaded)
{
    g_DatabaseReady = false;
    g_ServerId = 0;
    CloseDatabase(providerLoaded);
}
