#include "plugin.h"
#include "admin_menu.h"
#include "admins.h"
#include "api.h"
#include "clients.h"
#include "commands.h"
#include "effects.h"
#include "management_menu.h"
#include "panel_link.h"
#include "phrases.h"
#include "player_actions.h"
#include "punishments.h"
#include "settings.h"
#include "votes.h"
#include "storage.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include "filesystem.h"

Verdict g_Verdict;
PLUGIN_EXPOSE(Verdict, g_Verdict);

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars* gpGlobals = nullptr;

IUtilsApi* g_pUtils = nullptr;
IMenusApi* g_pMenus = nullptr;
IPlayersApi* g_pPlayers = nullptr;
IMySQLClient* g_pMysqlClient = nullptr;
IMySQLConnection* g_pConnection = nullptr;

static CTimer* g_SecondTimer = nullptr;
static int g_LastVoiceNotice[MAX_SLOTS];

CGameEntitySystem* GameEntitySystem()
{
    return g_pUtils->GetCGameEntitySystem();
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
    g_pUtils = (IUtilsApi*)g_SMAPI->MetaFactory(UTILS_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED)
    {
        UnloadSelf("Missing Utils system plugin (cs2-menus)");
        return false;
    }

    g_pMenus = (IMenusApi*)g_SMAPI->MetaFactory(MENUS_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED)
    {
        UnloadSelf("Missing Menus system plugin (cs2-menus)");
        return false;
    }

    g_pPlayers = (IPlayersApi*)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED)
    {
        UnloadSelf("Missing Players system plugin (cs2-menus)");
        return false;
    }

    ISQLInterface* sql = (ISQLInterface*)g_SMAPI->MetaFactory(SQLMM_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED)
    {
        UnloadSelf("Missing sql_mm plugin");
        return false;
    }
    g_pMysqlClient = sql->GetMySQLClient();
    return true;
}

bool Verdict::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
    PLUGIN_SAVEVARS();

    GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

    g_SMAPI->AddListener(this, this);
    g_pApi = new VerdictApi();

    ConVar_Register(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);
    return true;
}

bool Verdict::Unload(char* error, size_t maxlen)
{
    if (g_pUtils)
    {
        if (g_SecondTimer)
            g_pUtils->RemoveTimer(g_SecondTimer);
        ClearAllEffects();
        g_pUtils->ClearAllHooks(g_PLID);
    }
    ConVar_Unregister();

    if (g_pConnection)
    {
        g_pConnection->Destroy();
        g_pConnection = nullptr;
    }
    return true;
}

void Verdict::AllPluginsLoaded()
{
    if (!FindRequiredInterfaces())
        return;

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

    g_SecondTimer = g_pUtils->CreateTimer(1.0f, []() {
        CheckExpiredPunishments();
        CheckAdminExpiration();
        TickPlayerEffects();
        PanelLinkTick();
        return 1.0f;
    });

    ConnectDatabase();
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
    return "https://github.com/whyn0tq";
}

const char* Verdict::GetLicense()
{
    return "GPL";
}

const char* Verdict::GetVersion()
{
    return "2.0.0";
}

const char* Verdict::GetDate()
{
    return __DATE__;
}

const char* Verdict::GetLogTag()
{
    return "[Verdict]";
}
