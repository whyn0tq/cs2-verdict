#include "migration.h"
#include "clients.h"
#include "phrases.h"
#include "plugin.h"
#include "storage.h"
#include "common.h"

static const char* SteamIdPattern = "'^7656[0-9]{13}$'";

static std::string AccountExpression(const std::string& column)
{
    return "(CAST(" + column + " AS UNSIGNED) - 76561197960265728)";
}

static std::string AuthTailExpression(const std::string& column)
{
    std::string account = AccountExpression(column);
    return "CONCAT(MOD(" + account + ", 2), ':', FLOOR(" + account + " / 2))";
}

static std::string Steam2Expression(const std::string& column)
{
    return "CONCAT('STEAM_1:', " + AuthTailExpression(column) + ")";
}

static std::string AdminIdLookup(const std::string& sourceTable, const std::string& sourceIdColumn)
{
    return "IFNULL((SELECT x.`aid` FROM " + Table("admins") + " x JOIN " + sourceTable + " src ON SUBSTRING(x.`authid`, 9) = " +
        AuthTailExpression("src.`steamid`") + " WHERE src.`id` = " + sourceIdColumn + " AND src.`steamid` REGEXP " + SteamIdPattern + " LIMIT 1), 0)";
}

static std::string SourceTable(const std::string& prefix, const char* name)
{
    return "`" + prefix + name + "`";
}

static std::vector<std::string> BuildImportQueries(const std::string& prefix)
{
    std::string groups = SourceTable(prefix, "groups");
    std::string admins = SourceTable(prefix, "admins");
    std::string adminServers = SourceTable(prefix, "admins_servers");
    std::string punishments = SourceTable(prefix, "punishments");

    std::string missingAdmin = " AND NOT EXISTS (SELECT 1 FROM " + Table("admins") + " x WHERE SUBSTRING(x.`authid`, 9) = " + AuthTailExpression("a.`steamid`") + ")";

    std::string adminColumns = "SELECT {login}, " + Steam2Expression("a.`steamid`") + ", '', -1, '', 0, IFNULL(MAX(s.`immunity`), 0), MAX(g.`name`), "
        "LEFT(GROUP_CONCAT(IFNULL(s.`flags`, '') SEPARATOR ''), 64), IF(MIN(IFNULL(s.`expires`, 0)) = 0, 0, MAX(s.`expires`)), LEFT(MAX(a.`comment`), 128) "
        "FROM " + admins + " a LEFT JOIN " + adminServers + " s ON s.`admin_id` = a.`id` LEFT JOIN " + groups + " g ON g.`id` = s.`group_id` "
        "WHERE a.`steamid` REGEXP " + SteamIdPattern + missingAdmin + " GROUP BY a.`id`, a.`name`, a.`steamid`";

    std::string adminInsert = " (`user`, `authid`, `password`, `gid`, `email`, `extraflags`, `immunity`, `srv_group`, `srv_flags`, `expired`, `comment`) ";

    std::string firstPass = adminColumns;
    Replace(firstPass, "{login}", "LEFT(IFNULL(a.`name`, 'admin'), 64)");
    std::string secondPass = adminColumns;
    Replace(secondPass, "{login}", "LEFT(CONCAT(IFNULL(a.`name`, 'admin'), ' (', " + Steam2Expression("a.`steamid`") + ", ')'), 64)");

    std::string punishmentColumns =
        "SELECT {type}" + Steam2Expression("p.`steamid`") + ", LEFT(IFNULL(p.`name`, 'unnamed'), 128), IFNULL(p.`created`, 0), "
        "IF(IFNULL(p.`expires`, 0) = 0, IFNULL(p.`created`, 0), p.`expires`), IF(IFNULL(p.`expires`, 0) = 0, 0, p.`expires` - p.`created`), "
        "IFNULL(p.`reason`, ''), " + AdminIdLookup(admins, "p.`admin_id`") + ", '', IF(IFNULL(p.`server_id`, 0) < 0, 0, p.`server_id`), "
        "IF(p.`unpunish_admin_id` IS NULL, NULL, " + AdminIdLookup(admins, "p.`unpunish_admin_id`") + "), "
        "IF(p.`unpunish_admin_id` IS NULL, NULL, 'U'), IF(p.`unpunish_admin_id` IS NULL, NULL, UNIX_TIMESTAMP()) "
        "FROM " + punishments + " p WHERE p.`steamid` REGEXP " + SteamIdPattern;

    std::string bans = punishmentColumns;
    Replace(bans, "{type}", "0, IFNULL(p.`ip`, ''), ");
    std::string comms = punishmentColumns;
    Replace(comms, "{type}", "p.`punish_type`, ");

    std::vector<std::string> queries;

    queries.push_back("INSERT INTO " + Table("srvgroups") + " (`flags`, `immunity`, `name`, `groups_immune`, `maxbantime`, `maxmutetime`) "
        "SELECT LEFT(IFNULL(g.`flags`, ''), 30), IFNULL(g.`immunity`, 0), LEFT(g.`name`, 120), '', -1, -1 FROM " + groups + " g "
        "WHERE g.`name` IS NOT NULL AND g.`name` NOT IN (SELECT `name` FROM " + Table("srvgroups") + ")");

    queries.push_back("INSERT IGNORE INTO " + Table("admins") + adminInsert + firstPass);
    queries.push_back("INSERT IGNORE INTO " + Table("admins") + adminInsert + secondPass);

    queries.push_back("INSERT INTO " + Table("admins_servers_groups") + " (`admin_id`, `group_id`, `srv_group_id`, `server_id`) "
        "SELECT x.`aid`, IFNULL(MAX(sg.`id`), 0), -1, s.`server_id` FROM " + admins + " a "
        "JOIN " + adminServers + " s ON s.`admin_id` = a.`id` "
        "JOIN " + Table("admins") + " x ON SUBSTRING(x.`authid`, 9) = " + AuthTailExpression("a.`steamid`") + " "
        "LEFT JOIN " + groups + " g ON g.`id` = s.`group_id` "
        "LEFT JOIN " + Table("srvgroups") + " sg ON sg.`name` = g.`name` "
        "WHERE a.`steamid` REGEXP " + SteamIdPattern + " GROUP BY x.`aid`, s.`server_id`");

    queries.push_back("INSERT INTO " + Table("bans") +
        " (`type`, `ip`, `authid`, `name`, `created`, `ends`, `length`, `reason`, `aid`, `adminIp`, `sid`, `RemovedBy`, `RemoveType`, `RemovedOn`) " +
        bans + " AND p.`punish_type` = 0");

    queries.push_back("INSERT INTO " + Table("comms") +
        " (`type`, `authid`, `name`, `created`, `ends`, `length`, `reason`, `aid`, `adminIp`, `sid`, `RemovedBy`, `RemoveType`, `RemovedOn`) " +
        comms + " AND p.`punish_type` IN (1, 2, 3)");

    queries.push_back("INSERT INTO " + Table("settings") + " (`setting`, `value`) VALUES (" + Quote("verdict.import." + prefix) + ", UNIX_TIMESTAMP())");

    return queries;
}

static void ReportImport(std::vector<ISQLQuery*> results)
{
    const char* labels[] = { "groups", "admins", "admins (renamed)", "admin servers", "bans", "comms" };
    for (size_t i = 0; i < results.size() && i < 6; i++)
    {
        int rows = results[i] ? (int)results[i]->GetAffectedRows() : 0;
        META_CONPRINTF("[Verdict] Imported %s: %d\n", labels[i], rows);
    }
    META_CONPRINT("[Verdict] Import finished\n");

    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (IsRealClient(slot))
            SyncPlayer(slot);
    }
}

static void RunImport(const std::string& prefix)
{
    Transaction transaction;
    transaction.queries = BuildImportQueries(prefix);

    META_CONPRINTF("[Verdict] Importing cs2-admin_system data from %spunishments...\n", prefix.c_str());
    g_pConnection->ExecuteTransaction(transaction, ReportImport, [](std::string error, int code) {
        META_CONPRINTF("[Verdict] Import failed (%d): %s\n", code, error.c_str());
    });
}

static void CheckImportMarker(const std::string& prefix)
{
    std::string query = "SELECT COUNT(*) FROM " + Table("settings") + " WHERE `setting` = " + Quote("verdict.import." + prefix);
    RunQuery(query, [prefix](ISQLQuery* query) {
        ISQLResult* result = ResultOf(query);
        if (!result || !result->FetchRow())
            return;
        if (result->GetInt(0) > 0)
        {
            META_CONPRINTF("[Verdict] Data from %s tables was already imported\n", prefix.c_str());
            return;
        }
        RunImport(prefix);
    });
}

void ImportAdminSystem(const std::string& sourcePrefix)
{
    if (!IsDatabaseReady())
    {
        META_CONPRINT("[Verdict] Database is not ready yet\n");
        return;
    }

    std::string prefix = sourcePrefix;
    for (size_t i = 0; i < prefix.length(); i++)
    {
        char c = prefix[i];
        if (!isalnum((unsigned char)c) && c != '_')
        {
            META_CONPRINT("[Verdict] Invalid table prefix\n");
            return;
        }
    }

    std::string query = "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = " + Quote(prefix + "punishments");
    RunQuery(query, [prefix](ISQLQuery* query) {
        ISQLResult* result = ResultOf(query);
        if (!result || !result->FetchRow())
            return;
        if (result->GetInt(0) == 0)
        {
            META_CONPRINTF("[Verdict] Table %spunishments not found\n", prefix.c_str());
            return;
        }
        CheckImportMarker(prefix);
    });
}
