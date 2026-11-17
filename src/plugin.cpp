#include "plugin.h"
#include "admin_menu.h"
#include "admins.h"
#include "api.h"
#include "clients.h"
#include "commands.h"
#include "effects.h"
#include "management_menu.h"
#include "menu_owner.h"
#include "panel_link.h"
#include "phrases.h"
#include "player_actions.h"
#include "punishments.h"
#include "settings.h"
#include "votes.h"
#include "storage.h"
#include "timers.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include "filesystem/ifilesystem.h"

Verdict g_Verdict;
PLUGIN_EXPOSE(Verdict, g_Verdict);

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars* gpGlobals = nullptr;

IUtilsApi* g_pUtils = nullptr;
IMenusApi* g_pMenus = nullptr;
IPlayersApi* g_pPlayers = nullptr;

static Timers g_Timers;
static IMenuOwnerApi* g_MenuOwner = nullptr;
static PluginId g_UtilsPlugin = 0;
static int g_LastVoiceNotice[MAX_SLOTS];
static PluginId g_DatabasePlugin = 0;

void Schedule(float delay, std::function<float()> callback)
{
    g_Timers.Add(delay, std::move(callback), [](float interval, Timers::Callback run) {
        return g_pUtils->CreateTimer(interval, std::move(run));
    });
}

void DisplayMenu(Menu& menu, int slot)
{
    g_MenuOwner->DisplayMenu(g_PLID, menu, slot);
}

static void CatchEntitySystem()
{
    g_pGameEntitySystem = g_pUtils->GetCGameEntitySystem();
    g_pEntitySystem = g_pUtils->GetCEntitySystem();
    gpGlobals = g_pUtils->GetCGlobalVars();
}

static void UnloadSelf(const char* reason)
{
    ConColorMsg(Color(255, 0, 0, 255), "[Verdict] %s\n", reason);
    std::string command = "meta unload " + std::to_string(g_PLID);
    engine->ServerCommand(command.c_str());
}

void LoadClient(int slot, uint64 steamId)
{
    if (!IsValidSlot(slot) || steamId == 0 || g_pPlayers->IsFakeClient(slot))
        return;

    RememberClient(slot, steamId);
    ResetPunishments(slot);
    ResetAdmin(slot);
    ResetMenuState(slot);
    ResetManagementState(slot);
    ResetPlayerEffects(slot);
    g_LastVoiceNotice[slot] = 0;

    if (!IsDatabaseReady())
        return;

    CheckBan(slot, steamId);
    LoadComms(slot, steamId, false);
    LoadAdmin(slot, steamId);
}

void SyncPlayer(int slot)
{
    uint64 steamId = ClientSteamId(slot);
    if (steamId == 0)
        return;

    ResetPunishments(slot);
    CheckBan(slot, steamId);
    LoadComms(slot, steamId, true);
    LoadAdmin(slot, steamId);
}

void OnDatabaseReady()
{
    g_pApi->NotifyCoreLoaded();

    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (!g_pPlayers->IsConnected(slot) || g_pPlayers->IsFakeClient(slot) || !g_pPlayers->IsAuthenticated(slot))
            continue;
        LoadClient(slot, g_pPlayers->GetSteamID64(slot));
    }
}

static bool OnHearingClient(int slot)
{
    int type = ActiveVoiceType(slot);
    if (type == -1)
        return true;

    if (Now() - g_LastVoiceNotice[slot] >= 5)
    {
        g_LastVoiceNotice[slot] = Now();
        std::string message = Text("MuteActiveCenter");
        Replace(message, "{reason}", g_Punishments[slot][type].reason);
        Replace(message, "{time}", FormatTimeLeft(g_Punishments[slot][type].expires));
        g_pUtils->PrintToAlert(slot, "%s", StripColors(message).c_str());
    }
    return false;
}

static std::string ChatTextOf(const char* content)
{
    std::string text = content ? content : "";
    while (!text.empty() && (text[0] == '"' || text[0] == '\'' || text[0] == ' '))
        text.erase(0, 1);
    while (!text.empty() && (text[text.length() - 1] == '"' || text[text.length() - 1] == '\'' || text[text.length() - 1] == ' '))
        text.erase(text.length() - 1, 1);
    return text;
}

static bool OnChatMessage(int slot, const char* content, bool teamOnly)
{
    std::string text = ChatTextOf(content);

    if (HandleOwnReason(slot, text))
        return false;
    if (HandleMenuTextInput(slot, text))
        return false;

    int type = ActiveChatType(slot);
    bool command = !text.empty() && (text[0] == '!' || text[0] == '/');
    PanelChatMessage(slot, text, teamOnly, type != -1 && !(command && g_Settings.muteIgnoreCommands));
    if (type == -1)
        return true;

    if (command)
        return !g_Settings.muteIgnoreCommands;

    std::string message = Text("MuteActiveChat");
    Replace(message, "{reason}", g_Punishments[slot][type].reason);
    Replace(message, "{time}", FormatTimeLeft(g_Punishments[slot][type].expires));
    ChatTo(slot, message);
    return false;
}

static void ApplyStaticName(int slot)
{
    if (IsValidSlot(slot) && g_Admins[slot].id > 0 && !g_Admins[slot].name.empty())
        g_pPlayers->SetPlayerName(slot, g_Admins[slot].name.c_str());
}

static void HookStaticNames()
{
    g_pUtils->RegCommand(g_PLID, { "jointeam" }, std::vector<std::string>(), [](int slot, const char*) {
        if (g_Settings.staticNames)
            ApplyStaticName(slot);
        return false;
    });
    g_pUtils->HookEvent(g_PLID, "round_start", [](const char*, IGameEvent*, bool) {
        if (!g_Settings.staticNames)
            return;
        for (int slot = 0; slot < MAX_SLOTS; slot++)
            ApplyStaticName(slot);
    });
}

static bool FindRequiredInterfaces()
{
    int ret = 0;
    g_pUtils = static_cast<IUtilsApi*>(g_SMAPI->MetaFactory(UTILS_INTERFACE, &ret, &g_UtilsPlugin));
    if (!g_pUtils || ret != META_IFACE_OK)
    {
        UnloadSelf("Missing Utils system plugin (cs2-menus)");
        return false;
    }

    PluginId menusPlugin = 0;
    g_pMenus = static_cast<IMenusApi*>(g_SMAPI->MetaFactory(MENUS_INTERFACE, &ret, &menusPlugin));
    if (!g_pMenus || ret != META_IFACE_OK || menusPlugin != g_UtilsPlugin)
    {
        UnloadSelf("Missing Menus system plugin (cs2-menus)");
        return false;
    }

    PluginId playersPlugin = 0;
    g_pPlayers = static_cast<IPlayersApi*>(g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, &playersPlugin));
    if (!g_pPlayers || ret != META_IFACE_OK || playersPlugin != g_UtilsPlugin)
    {
        UnloadSelf("Missing Players system plugin (cs2-menus)");
        return false;
    }

    PluginId ownerPlugin = 0;
    g_MenuOwner = static_cast<IMenuOwnerApi*>(g_SMAPI->MetaFactory(MENU_OWNER_INTERFACE, &ret, &ownerPlugin));
    if (!g_MenuOwner || ret != META_IFACE_OK || ownerPlugin != g_UtilsPlugin)
    {
        UnloadSelf("Missing menu ownership support from cs2-menus 2.0.0+");
        return false;
    }
    return true;
}

bool Verdict::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
    PLUGIN_SAVEVARS();

    GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

    g_SMAPI->AddListener(this, this);
    g_pApi = new VerdictApi();

    ConVar_Register(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);
    return true;
}

bool Verdict::Unload(char* error, size_t maxlen)
{
    PluginId databasePlugin = 0;
    g_SMAPI->MetaFactory(SQL_INTERFACE_V2, nullptr, &databasePlugin);
    DisconnectDatabase(databasePlugin != 0 && databasePlugin == g_DatabasePlugin);

    PluginId utilsPlugin = 0;
    g_SMAPI->MetaFactory(UTILS_INTERFACE, nullptr, &utilsPlugin);
    if (g_pUtils && utilsPlugin != 0 && utilsPlugin == g_UtilsPlugin)
    {
        if (g_MenuOwner)
            g_MenuOwner->CloseMenus(g_PLID);
        g_Timers.Clear([](CTimer* timer) { g_pUtils->RemoveTimer(timer); });
        ClearAllEffects();
        g_pUtils->ClearAllHooks(g_PLID);
    }
    else
        g_Timers.Forget();
    ConVar_Unregister();

    return true;
}

void Verdict::AllPluginsLoaded()
{
    if (!FindRequiredInterfaces())
        return;

    int ret = 0;
    auto* database = static_cast<sql::IApiV2*>(g_SMAPI->MetaFactory(SQL_INTERFACE_V2, &ret, &g_DatabasePlugin));
    if (!database || ret != META_IFACE_OK)
    {
        UnloadSelf("Missing cs2-DBMP 1.3.0+");
        return;
    }

    CatchEntitySystem();
    LoadSettings();
    LoadPhrases();

    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        ResetAdmin(slot);
        ResetPunishments(slot);
        ResetMenuState(slot);
        ResetManagementState(slot);
        ResetPlayerEffects(slot);
    }

    RegisterManagementMenuItems();
    RegisterVoteMenuItems();
    RegisterCoreMenuItems();
    RegisterChatCommands();
    HookStaticNames();

    g_pPlayers->AddListener(g_PLID, this);
    g_pUtils->StartupServer(g_PLID, CatchEntitySystem);
    g_pUtils->HookIsHearingClient(g_PLID, OnHearingClient);
    g_pUtils->AddChatListenerPre(g_PLID, OnChatMessage);

    Schedule(1.0f, []() {
        CheckExpiredPunishments();
        CheckAdminExpiration();
        TickPlayerEffects();
        PanelLinkTick();
        return 1.0f;
    });

    ConnectDatabase(*database);
}

void* Verdict::OnMetamodQuery(const char* iface, int* ret)
{
    if (!strcmp(iface, Admin_INTERFACE))
    {
        *ret = META_IFACE_OK;
        return static_cast<IAdminApi*>(g_pApi);
    }
    if (!strcmp(iface, VERDICT_INTERFACE))
    {
        *ret = META_IFACE_OK;
        return static_cast<IVerdictApi*>(g_pApi);
    }

    *ret = META_IFACE_FAILED;
    return nullptr;
}

void Verdict::OnPluginUnload(PluginId id)
{
    if (g_UtilsPlugin && id == g_UtilsPlugin)
    {
        g_UtilsPlugin = 0;
        DisconnectDatabase();
        UnloadSelf("cs2-menus was unloaded");
    }
    if (g_DatabasePlugin && id == g_DatabasePlugin)
    {
        g_DatabasePlugin = 0;
        DisconnectDatabase(false);
        UnloadSelf("cs2-DBMP was unloaded");
    }
    if (g_pApi)
        g_pApi->ForgetPlugin(id);
}

void Verdict::OnClientAuthorized(int iSlot, uint64 iSteamID64)
{
    LoadClient(iSlot, iSteamID64);
    if (IsRealClient(iSlot))
        PanelPlayerJoined(iSlot);
}

void Verdict::ClientDisconnect(int iSlot)
{
    if (!IsValidSlot(iSlot))
        return;

    uint64 steamId = ClientSteamId(iSlot);
    PanelPlayerLeft(iSlot);
    ForgetClient(iSlot);

    RecentClient* recent = FindRecentClient(steamId);
    if (recent)
    {
        for (int type = RT_BAN; type <= RT_SILENCE; type++)
            recent->punished[type] = IsPunished(iSlot, type);
    }

    ResetPunishments(iSlot);
    ResetAdmin(iSlot);
    ResetMenuState(iSlot);
    ResetManagementState(iSlot);
    ResetPlayerEffects(iSlot);
}

const char* Verdict::GetAuthor()
{
    return "teesa (whyn0tq)";
}

const char* Verdict::GetName()
{
    return "Verdict";
}

const char* Verdict::GetDescription()
{
    return "Bans, comms and admins with a Material Admin compatible database";
}

const char* Verdict::GetURL()
{
    return "https://github.com/whyn0tq/cs2-verdict-v2";
}

const char* Verdict::GetLicense()
{
    return "GPL";
}

const char* Verdict::GetVersion()
{
    return "1.2.0";
}

const char* Verdict::GetDate()
{
    return __DATE__;
}

const char* Verdict::GetLogTag()
{
    return "[Verdict]";
}
