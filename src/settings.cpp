#include "settings.h"
#include "common.h"
#include "filesystem.h"

Settings g_Settings;

static const char* g_SectionNames[4] = { "bans", "mutes", "gags", "silences" };

const char* PunishSection(int type)
{
    if (type < RT_BAN || type > RT_SILENCE)
        return "";
    return g_SectionNames[type];
}

static void ReadTimeList(KeyValues* section, std::vector<TimeOption>& times)
{
    FOR_EACH_VALUE(section, value)
    {
        TimeOption option;
        option.seconds = atoi(value->GetName());
        option.title = value->GetString(nullptr, "");
        times.push_back(option);
    }
}

static void ReadReasonList(KeyValues* section, std::vector<std::string>& reasons)
{
    FOR_EACH_VALUE(section, value)
    {
        if (value->GetInt(nullptr, 0))
            reasons.push_back(value->GetName());
    }
}

static void ReadReasonTimes(KeyValues* section, std::vector<ReasonOption>& reasons)
{
    FOR_EACH_SUBKEY(section, reasonKey)
    {
        ReasonOption option;
        option.reason = reasonKey->GetName();
        ReadTimeList(reasonKey, option.times);
        reasons.push_back(option);
    }
}

static void ReadFlags(KeyValues* config)
{
    KeyValues* flags = config->FindKey("flags", false);
    if (!flags)
        return;

    FOR_EACH_SUBKEY(flags, flagKey)
    {
        FlagInfo flag;
        flag.name = flagKey->GetString("name", flagKey->GetName());
        KeyValues* access = flagKey->FindKey("access", false);
        if (access)
        {
            FOR_EACH_VALUE(access, permission)
            {
                if (permission->GetInt(nullptr, 0))
                    flag.permissions.push_back(permission->GetName());
            }
        }
        g_Settings.flags[flagKey->GetName()] = flag;
    }
}

static void ReadDefaultPermissions(KeyValues* config)
{
    KeyValues* defaults = config->FindKey("default_permissions", false);
    if (!defaults)
        return;

    FOR_EACH_VALUE(defaults, permission)
    {
        if (permission->GetInt(nullptr, 0))
            g_Settings.defaultPermissions.push_back(permission->GetName());
    }
}

static void ReadPunishOptions(KeyValues* config)
{
    KeyValues* reasonTimes = config->FindKey("times_reasons", false);
    KeyValues* reasons = config->FindKey("reasons", false);
    KeyValues* times = config->FindKey("times", false);

    for (int type = RT_BAN; type <= RT_SILENCE; type++)
    {
        const char* section = PunishSection(type);

        if (reasonTimes && reasonTimes->FindKey(section, false))
            ReadReasonTimes(reasonTimes->FindKey(section, false), g_Settings.reasonTimes[type]);

        if (reasons && reasons->FindKey(section, false))
            ReadReasonList(reasons->FindKey(section, false), g_Settings.reasons[type]);

        if (times && times->FindKey(section, false))
            ReadTimeList(times->FindKey(section, false), g_Settings.times[type]);
    }
}

static void ReadNamedOptions(KeyValues* section, std::vector<NamedOption>& options)
{
    if (!section)
        return;
    FOR_EACH_VALUE(section, value)
    {
        NamedOption option;
        option.value = value->GetName();
        option.title = value->GetString(nullptr, value->GetName());
        options.push_back(option);
    }
}

static void ReadEffects(KeyValues* config)
{
    KeyValues* section = config->FindKey("effects", true);
    EffectSettings& effects = g_Settings.effects;
    effects.fireParticle = section->GetString("fire_particle", "particles/burning_fx/burning_character.vpcf");
    effects.explosionParticle = section->GetString("explosion_particle", "particles/explosions_fx/explosion_c4_500.vpcf");
    effects.igniteSound = section->GetString("ignite_sound", "Inferno.Fire.Ignite");
    effects.burnSound = section->GetString("burn_sound", "Player.BurnDamage");
    effects.slapSound = section->GetString("slap_sound", "Player.DamageFall");
    effects.freezeSound = section->GetString("freeze_sound", "Glass.BulletImpact");
    effects.bombTickSound = section->GetString("bomb_tick_sound", "coop.c4_beep_normal");
    effects.bombLastTickSound = section->GetString("bomb_last_tick_sound", "coop.c4_beep_loud");
    effects.explosionSound = section->GetString("explosion_sound", "c4.explode");
    effects.bombTicks = section->GetInt("bomb_ticks", 10);
    effects.freezeBombSeconds = section->GetInt("freezebomb_seconds", 10);
    effects.fireBombSeconds = section->GetInt("firebomb_seconds", 20);
}

static void LoadMaps()
{
    KeyValues* maps = new KeyValues("Maps");
    if (maps->LoadFromFile(g_pFullFileSystem, "addons/configs/verdict/maps.ini"))
        ReadNamedOptions(maps, g_Settings.maps);
    delete maps;
}

static void LoadSorting()
{
    KeyValues* sorting = new KeyValues("Menu");
    if (!sorting->LoadFromFile(g_pFullFileSystem, "addons/configs/verdict/sorting.ini"))
    {
        delete sorting;
        return;
    }

    FOR_EACH_SUBKEY(sorting, category)
    {
        std::string name = category->GetName();
        g_Settings.categoryOrder.push_back(name);
        FOR_EACH_VALUE(category, item)
        {
            g_Settings.itemOrder[name].push_back(item->GetString(nullptr, ""));
        }
    }
    delete sorting;
}

bool LoadSettings()
{
    g_Settings = Settings();

    KeyValues* config = new KeyValues("Config");
    if (!config->LoadFromFile(g_pFullFileSystem, "addons/configs/verdict/core.ini"))
    {
        g_pUtils->ErrorLog("[Verdict] Failed to load addons/configs/verdict/core.ini");
        delete config;
        return false;
    }

    g_Settings.tablePrefix = config->GetString("database_prefix", "sb");
    g_Settings.serverId = config->GetInt("server_id", -1);
    g_Settings.serverAddress = config->GetString("server_address", "");
    g_Settings.chatPrefix = config->GetString("chat_prefix", " {LIGHTRED}[Verdict]{DEFAULT}");
    g_Settings.punishScope = config->GetInt("punish_scope", 0);
    g_Settings.checkIpOnSteamBans = config->GetBool("check_ip_on_steam_bans", false);
    g_Settings.storeIp = config->GetBool("punish_ip", true);
    g_Settings.timeReasonType = config->GetInt("time_reason_type", 1);
    g_Settings.notifyType = config->GetInt("notify_type", 1);
    g_Settings.immunityType = config->GetInt("immunity_type", 1);
    g_Settings.banDelay = config->GetInt("ban_delay", 5);
    g_Settings.unpunishType = config->GetInt("unpunish_type", 0);
    g_Settings.offlinePunishCount = config->GetInt("punish_offline_count", 30);
    g_Settings.offlineUnpunishCount = config->GetInt("unpunish_offline_count", 30);
    g_Settings.messageType = config->GetInt("message_type", 2);
    g_Settings.staticNames = config->GetBool("static_name", false);
    g_Settings.muteIgnoreCommands = config->GetBool("mute_ignore_commands", true);
    g_Settings.ownReasons = config->GetBool("own_reason", true);
    g_Settings.mapChangeDelay = config->GetInt("map_change_delay", 3);
    g_Settings.voteDuration = config->GetInt("vote_duration", 20);
    g_Settings.votePercent = config->GetInt("vote_percent", 60);
    g_Settings.voteBanMinutes = config->GetInt("vote_ban_minutes", 30);
    g_Settings.voteMuteMinutes = config->GetInt("vote_mute_minutes", 30);

    ReadDefaultPermissions(config);
    ReadFlags(config);
    ReadPunishOptions(config);
    ReadNamedOptions(config->FindKey("configs", false), g_Settings.configs);
    LoadMaps();
    ReadEffects(config);
    LoadSorting();

    delete config;
    return true;
}
