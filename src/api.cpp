#include "api.h"
#include "admin_menu.h"
#include "admins.h"
#include "clients.h"
#include "phrases.h"
#include "plugin.h"
#include "punishments.h"
#include "settings.h"
#include "steam.h"
#include "storage.h"
#include "common.h"
#include <algorithm>

VerdictApi* g_pApi = nullptr;

static uint64 SteamIdFromText(const char* text)
{
    uint64 steamId = 0;
    if (!text || !ParseSteamId(text, steamId))
        return 0;
    return steamId;
}

float VerdictApi::GetPluginVersion()
{
    return (float)atof(g_PLAPI->GetVersion());
}

const char* VerdictApi::GetTranslation(const char* szKey)
{
    if (!szKey)
        return "";
    return Phrase(szKey);
}

IMySQLConnection* VerdictApi::GetMySQLConnection()
{
    return g_pConnection;
}

void VerdictApi::RegisterCategory(const char* szIdentity, const char* szName, OnCategoryDisplayCallback callback)
{
    RegisterMenuCategory(szIdentity, szName, callback);
}

void VerdictApi::RegisterItem(const char* szIdentity, const char* szName, const char* szCategory, const char* szFlag, OnItemDisplayCallback callbackDisplay, OnItemSelectCallback callbackSelect)
{
    RegisterMenuItem(szIdentity, szName, szCategory, szFlag, callbackDisplay, callbackSelect);
}

bool VerdictApi::HasPermission(int iSlot, const char* szPermission)
{
    return ::HasPermission(iSlot, szPermission);
}

bool VerdictApi::HasFlag(int iSlot, const char* szFlag)
{
    return ::HasFlag(iSlot, szFlag);
}

bool VerdictApi::IsAdmin(int iSlot)
{
    return ::IsAdmin(iSlot);
}

void VerdictApi::OnAdminConnected(SourceMM::PluginId id, OnAdminConnectedCallback callback)
{
    m_adminConnected[id] = callback;
}

const char* VerdictApi::GetFlagName(const char* szFlag)
{
    if (!szFlag)
        return "";
    std::map<std::string, FlagInfo>::iterator found = g_Settings.flags.find(szFlag);
    if (found == g_Settings.flags.end())
        return "";
    return found->second.name.c_str();
}

std::vector<std::string> VerdictApi::GetPermissionsByFlag(const char* szFlag)
{
    if (!szFlag)
        return std::vector<std::string>();
    return PermissionsForFlag(szFlag);
}

std::vector<std::string> VerdictApi::GetAdminFlags(int iSlot)
{
    if (!IsValidSlot(iSlot))
        return std::vector<std::string>();
    return g_Admins[iSlot].flags;
}

std::vector<std::string> VerdictApi::GetAdminPermissions(int iSlot)
{
    if (!IsValidSlot(iSlot))
        return std::vector<std::string>();
    return g_Admins[iSlot].permissions;
}

int VerdictApi::GetAdminImmunity(int iSlot)
{
    return AdminImmunity(iSlot);
}

int VerdictApi::GetAdminExpireTime(int iSlot)
{
    if (!IsValidSlot(iSlot))
        return -1;
    return g_Admins[iSlot].expires;
}

bool VerdictApi::IsPlayerPunished(int iSlot, int iType)
{
    return IsPunished(iSlot, iType);
}

int VerdictApi::GetPlayerPunishmentExpired(int iSlot, int iType)
{
    if (!IsValidSlot(iSlot) || iType < RT_BAN || iType > RT_SILENCE)
        return -1;
    return g_Punishments[iSlot][iType].expires;
}

const char* VerdictApi::GetPlayerPunishmentReason(int iSlot, int iType)
{
    if (!IsValidSlot(iSlot) || iType < RT_BAN || iType > RT_SILENCE)
        return "";
    return g_Punishments[iSlot][iType].reason.c_str();
}

void VerdictApi::AddPlayerPunishment(int iSlot, int iType, int iTime, const char* szReason, int iAdminID, bool bNotify, bool bDB)
{
    PunishPlayer(iSlot, iType, iTime, szReason ? szReason : "", iAdminID, bNotify, bDB);
}

void VerdictApi::OnPlayerPunish(SourceMM::PluginId id, OnPlayerPunishCallback callback)
{
    m_punish[id] = callback;
}

void VerdictApi::OnOfflinePlayerPunish(SourceMM::PluginId id, OnOfflinePlayerPunishCallback callback)
{
    m_offlinePunish[id] = callback;
}

void VerdictApi::AddOfflinePlayerPunishment(const char* szSteamID64, const char* szName, int iType, int iTime, const char* szReason, int iAdminID)
{
    uint64 steamId = SteamIdFromText(szSteamID64);
    if (steamId == 0)
        return;
    PunishOffline(steamId, szName ? szName : "", iType, iTime, szReason ? szReason : "", iAdminID);
}

void VerdictApi::RemovePlayerPunishment(int iSlot, int iType, int iAdminID, bool bNotify)
{
    UnpunishPlayer(iSlot, iType, iAdminID, bNotify, "");
}

void VerdictApi::RemoveOfflinePlayerPunishment(const char* szSteamID64, int iType, int iAdminID)
{
    uint64 steamId = SteamIdFromText(szSteamID64);
    if (steamId == 0)
        return;
    UnpunishOffline(steamId, iType, iAdminID, "", false);
}

void VerdictApi::OnPlayerUnpunish(SourceMM::PluginId id, OnPlayerUnpunishCallback callback)
{
    m_unpunish[id] = callback;
}

void VerdictApi::OnOfflinePlayerUnpunish(SourceMM::PluginId id, OnOfflinePlayerUnpunishCallback callback)
{
    m_offlineUnpunish[id] = callback;
}

void VerdictApi::ShowAdminMenu(int iSlot)
{
    ::ShowAdminMenu(iSlot);
}

void VerdictApi::ShowAdminCategoryMenu(int iSlot, const char* szCategory)
{
    if (szCategory)
        ShowCategory(iSlot, szCategory);
}

void VerdictApi::ShowAdminLastCategoryMenu(int iSlot)
{
    ShowLastCategory(iSlot);
}

void VerdictApi::ShowAdminItemMenu(int iSlot, const char* szCategory, const char* szIdentity)
{
    if (szCategory && szIdentity)
        ShowItem(iSlot, szCategory, szIdentity);
}

void VerdictApi::AddPlayerAdmin(const char* szName, const char* szSteamID64, const char* szFlags, int iImmunity, int iExpireTime, int iGroupID, const char* szComment, bool bDB)
{
    uint64 steamId = SteamIdFromText(szSteamID64);
    if (steamId == 0)
        return;
    AddAdmin(szName ? szName : "", steamId, szFlags ? szFlags : "", iImmunity, iExpireTime, iGroupID, szComment ? szComment : "", bDB);
}

void VerdictApi::RemovePlayerAdmin(const char* szSteamID64, bool bDB)
{
    uint64 steamId = SteamIdFromText(szSteamID64);
    if (steamId != 0)
        RemoveAdmin(steamId, bDB);
}

void VerdictApi::AddPlayerLocalFlag(int iSlot, const char* szFlag)
{
    if (IsValidSlot(iSlot) && szFlag)
        GrantFlag(g_Admins[iSlot], szFlag);
}

void VerdictApi::RemovePlayerLocalFlag(int iSlot, const char* szFlag)
{
    if (!IsValidSlot(iSlot) || !szFlag)
        return;
    std::vector<std::string>& flags = g_Admins[iSlot].flags;
    flags.erase(std::remove(flags.begin(), flags.end(), std::string(szFlag)), flags.end());
}

void VerdictApi::AddPlayerLocalPermission(int iSlot, const char* szPermission)
{
    if (!IsValidSlot(iSlot) || !szPermission)
        return;
    std::vector<std::string>& permissions = g_Admins[iSlot].permissions;
    if (std::find(permissions.begin(), permissions.end(), szPermission) == permissions.end())
        permissions.push_back(szPermission);
}

void VerdictApi::RemovePlayerLocalPermission(int iSlot, const char* szPermission)
{
    if (!IsValidSlot(iSlot) || !szPermission)
        return;
    std::vector<std::string>& permissions = g_Admins[iSlot].permissions;
    permissions.erase(std::remove(permissions.begin(), permissions.end(), std::string(szPermission)), permissions.end());
}

void VerdictApi::AddPlayerLocalImmunity(int iSlot, int iImmunity)
{
    if (IsValidSlot(iSlot))
        g_Admins[iSlot].immunity = iImmunity;
}

void VerdictApi::RemovePlayerLocalImmunity(int iSlot)
{
    if (IsValidSlot(iSlot))
        g_Admins[iSlot].immunity = 0;
}

void VerdictApi::OnCoreLoaded(SourceMM::PluginId id, OnCoreLoadedCallback callback)
{
    m_coreLoadedCallbacks[id] = callback;
}

bool VerdictApi::IsCoreLoaded()
{
    return m_coreLoaded;
}

void VerdictApi::OnCategoryRegister(SourceMM::PluginId id, OnCategoryRegisterCallback callback)
{
    m_categoryRegister[id] = callback;
}

void VerdictApi::SendAction(int iSlot, const char* szAction, const char* szParam)
{
    std::map<SourceMM::PluginId, OnActionCallback>::iterator it;
    for (it = m_actions.begin(); it != m_actions.end(); ++it)
        it->second(iSlot, szAction, szParam);
}

void VerdictApi::OnAction(SourceMM::PluginId id, OnActionCallback callback)
{
    m_actions[id] = callback;
}

int VerdictApi::GetMessageType()
{
    return g_Settings.messageType;
}

void VerdictApi::OnPlayerPunishPre(SourceMM::PluginId id, OnPlayerPunishCallbackPre callback)
{
    m_punishPre[id] = callback;
}

void VerdictApi::OnOfflinePlayerPunishPre(SourceMM::PluginId id, OnOfflinePlayerPunishCallbackPre callback)
{
    m_offlinePunishPre[id] = callback;
}

const char* VerdictApi::GetAdminName(int iSlot)
{
    static std::string name;
    name = AdminName(iSlot);
    return name.c_str();
}

int VerdictApi::GetAdminGroupID(int iSlot)
{
    if (!IsValidSlot(iSlot) || g_Admins[iSlot].id <= 0)
        return -1;
    return g_Admins[iSlot].groupId;
}

const char* VerdictApi::GetAdminGroupName(int iSlot)
{
    if (!IsValidSlot(iSlot))
        return "";
    return g_Admins[iSlot].groupName.c_str();
}

int VerdictApi::GetImmunityType()
{
    return g_Settings.immunityType;
}

int VerdictApi::GetServerID()
{
    return ServerId();
}

const char* VerdictApi::GetTablePrefix()
{
    static std::string prefix;
    prefix = TablePrefix();
    return prefix.c_str();
}

int VerdictApi::GetAdminDatabaseID(int iSlot)
{
    return AdminDatabaseId(iSlot);
}

void VerdictApi::ReloadPlayer(int iSlot)
{
    if (IsRealClient(iSlot))
        SyncPlayer(iSlot);
}

void VerdictApi::KickPlayer(int iSlot, const char* szReason, int iAdminID)
{
    if (IsRealClient(iSlot))
        KickWithAnnounce(iSlot, iAdminID, szReason ? szReason : "");
}

void VerdictApi::NotifyAdminConnected(int slot)
{
    std::map<SourceMM::PluginId, OnAdminConnectedCallback>::iterator it;
    for (it = m_adminConnected.begin(); it != m_adminConnected.end(); ++it)
        it->second(slot);
}

void VerdictApi::NotifyCoreLoaded()
{
    m_coreLoaded = true;
    std::map<SourceMM::PluginId, OnCoreLoadedCallback>::iterator it;
    for (it = m_coreLoadedCallbacks.begin(); it != m_coreLoadedCallbacks.end(); ++it)
        it->second();
}

void VerdictApi::NotifyCategoryRegistered(const char* category)
{
    std::map<SourceMM::PluginId, OnCategoryRegisterCallback>::iterator it;
    for (it = m_categoryRegister.begin(); it != m_categoryRegister.end(); ++it)
        it->second(category);
}

bool VerdictApi::AskPunish(int slot, int type, int seconds, const char* reason, int adminSlot)
{
    std::map<SourceMM::PluginId, OnPlayerPunishCallbackPre>::iterator it;
    for (it = m_punishPre.begin(); it != m_punishPre.end(); ++it)
    {
        if (it->second(slot, type, seconds, reason, adminSlot))
            return true;
    }
    return false;
}

void VerdictApi::NotifyPunish(int slot, int type, int seconds, const char* reason, int adminSlot)
{
    std::map<SourceMM::PluginId, OnPlayerPunishCallback>::iterator it;
    for (it = m_punish.begin(); it != m_punish.end(); ++it)
        it->second(slot, type, seconds, reason, adminSlot);
}

bool VerdictApi::AskOfflinePunish(const char* steamId, const char* name, int type, int seconds, const char* reason, int adminSlot)
{
    std::map<SourceMM::PluginId, OnOfflinePlayerPunishCallbackPre>::iterator it;
    for (it = m_offlinePunishPre.begin(); it != m_offlinePunishPre.end(); ++it)
    {
        if (it->second(steamId, name, type, seconds, reason, adminSlot))
            return true;
    }
    return false;
}

void VerdictApi::NotifyOfflinePunish(const char* steamId, const char* name, int type, int seconds, const char* reason, int adminSlot)
{
    std::map<SourceMM::PluginId, OnOfflinePlayerPunishCallback>::iterator it;
    for (it = m_offlinePunish.begin(); it != m_offlinePunish.end(); ++it)
        it->second(steamId, name, type, seconds, reason, adminSlot);
}

void VerdictApi::NotifyUnpunish(int slot, int type, int adminSlot)
{
    std::map<SourceMM::PluginId, OnPlayerUnpunishCallback>::iterator it;
    for (it = m_unpunish.begin(); it != m_unpunish.end(); ++it)
        it->second(slot, type, adminSlot);
}

void VerdictApi::NotifyOfflineUnpunish(const char* steamId, int type, int adminSlot)
{
    std::map<SourceMM::PluginId, OnOfflinePlayerUnpunishCallback>::iterator it;
    for (it = m_offlineUnpunish.begin(); it != m_offlineUnpunish.end(); ++it)
        it->second(steamId, type, adminSlot);
}

void VerdictApi::ForgetPlugin(SourceMM::PluginId id)
{
    m_adminConnected.erase(id);
    m_punish.erase(id);
    m_punishPre.erase(id);
    m_offlinePunish.erase(id);
    m_offlinePunishPre.erase(id);
    m_unpunish.erase(id);
    m_offlineUnpunish.erase(id);
    m_coreLoadedCallbacks.erase(id);
    m_categoryRegister.erase(id);
    m_actions.erase(id);
}
