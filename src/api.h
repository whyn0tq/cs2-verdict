#pragma once

#include <ISmmPlugin.h>
#include <map>
#include "verdict.h"

class VerdictApi : public IVerdictApi
{
public:
    float GetPluginVersion() override;
    const char* GetTranslation(const char* szKey) override;
    IMySQLConnection* GetMySQLConnection() override;
    void RegisterCategory(const char* szIdentity, const char* szName, OnCategoryDisplayCallback callback) override;
    void RegisterItem(const char* szIdentity, const char* szName, const char* szCategory, const char* szFlag, OnItemDisplayCallback callbackDisplay, OnItemSelectCallback callbackSelect) override;
    bool HasPermission(int iSlot, const char* szPermission) override;
    bool HasFlag(int iSlot, const char* szFlag) override;
    bool IsAdmin(int iSlot) override;
    void OnAdminConnected(SourceMM::PluginId id, OnAdminConnectedCallback callback) override;
    const char* GetFlagName(const char* szFlag) override;
    std::vector<std::string> GetPermissionsByFlag(const char* szFlag) override;
    std::vector<std::string> GetAdminFlags(int iSlot) override;
    std::vector<std::string> GetAdminPermissions(int iSlot) override;
    int GetAdminImmunity(int iSlot) override;
    int GetAdminExpireTime(int iSlot) override;
    bool IsPlayerPunished(int iSlot, int iType) override;
    int GetPlayerPunishmentExpired(int iSlot, int iType) override;
    const char* GetPlayerPunishmentReason(int iSlot, int iType) override;
    void AddPlayerPunishment(int iSlot, int iType, int iTime, const char* szReason, int iAdminID, bool bNotify, bool bDB) override;
    void OnPlayerPunish(SourceMM::PluginId id, OnPlayerPunishCallback callback) override;
    void OnOfflinePlayerPunish(SourceMM::PluginId id, OnOfflinePlayerPunishCallback callback) override;
    void AddOfflinePlayerPunishment(const char* szSteamID64, const char* szName, int iType, int iTime, const char* szReason, int iAdminID) override;
    void RemovePlayerPunishment(int iSlot, int iType, int iAdminID, bool bNotify) override;
    void RemoveOfflinePlayerPunishment(const char* szSteamID64, int iType, int iAdminID) override;
    void OnPlayerUnpunish(SourceMM::PluginId id, OnPlayerUnpunishCallback callback) override;
    void OnOfflinePlayerUnpunish(SourceMM::PluginId id, OnOfflinePlayerUnpunishCallback callback) override;
    void ShowAdminMenu(int iSlot) override;
    void ShowAdminCategoryMenu(int iSlot, const char* szCategory) override;
    void ShowAdminLastCategoryMenu(int iSlot) override;
    void ShowAdminItemMenu(int iSlot, const char* szCategory, const char* szIdentity) override;
    void AddPlayerAdmin(const char* szName, const char* szSteamID64, const char* szFlags, int iImmunity, int iExpireTime, int iGroupID, const char* szComment, bool bDB) override;
    void RemovePlayerAdmin(const char* szSteamID64, bool bDB) override;
    void AddPlayerLocalFlag(int iSlot, const char* szFlag) override;
    void RemovePlayerLocalFlag(int iSlot, const char* szFlag) override;
    void AddPlayerLocalPermission(int iSlot, const char* szPermission) override;
    void RemovePlayerLocalPermission(int iSlot, const char* szPermission) override;
    void AddPlayerLocalImmunity(int iSlot, int iImmunity) override;
    void RemovePlayerLocalImmunity(int iSlot) override;
    void OnCoreLoaded(SourceMM::PluginId id, OnCoreLoadedCallback callback) override;
    bool IsCoreLoaded() override;
    void OnCategoryRegister(SourceMM::PluginId id, OnCategoryRegisterCallback callback) override;
    void SendAction(int iSlot, const char* szAction, const char* szParam) override;
    void OnAction(SourceMM::PluginId id, OnActionCallback callback) override;
    int GetMessageType() override;
    void OnPlayerPunishPre(SourceMM::PluginId id, OnPlayerPunishCallbackPre callback) override;
    void OnOfflinePlayerPunishPre(SourceMM::PluginId id, OnOfflinePlayerPunishCallbackPre callback) override;
    const char* GetAdminName(int iSlot) override;
    int GetAdminGroupID(int iSlot) override;
    const char* GetAdminGroupName(int iSlot) override;
    int GetImmunityType() override;

    int GetServerID() override;
    const char* GetTablePrefix() override;
    int GetAdminDatabaseID(int iSlot) override;
    void ReloadPlayer(int iSlot) override;
    void KickPlayer(int iSlot, const char* szReason, int iAdminID) override;

    void NotifyAdminConnected(int slot);
    void NotifyCoreLoaded();
    void NotifyCategoryRegistered(const char* category);
    bool AskPunish(int slot, int type, int seconds, const char* reason, int adminSlot);
    void NotifyPunish(int slot, int type, int seconds, const char* reason, int adminSlot);
    bool AskOfflinePunish(const char* steamId, const char* name, int type, int seconds, const char* reason, int adminSlot);
    void NotifyOfflinePunish(const char* steamId, const char* name, int type, int seconds, const char* reason, int adminSlot);
    void NotifyUnpunish(int slot, int type, int adminSlot);
    void NotifyOfflineUnpunish(const char* steamId, int type, int adminSlot);
    void ForgetPlugin(SourceMM::PluginId id);

private:
    std::map<SourceMM::PluginId, OnAdminConnectedCallback> m_adminConnected;
    std::map<SourceMM::PluginId, OnPlayerPunishCallback> m_punish;
    std::map<SourceMM::PluginId, OnPlayerPunishCallbackPre> m_punishPre;
    std::map<SourceMM::PluginId, OnOfflinePlayerPunishCallback> m_offlinePunish;
    std::map<SourceMM::PluginId, OnOfflinePlayerPunishCallbackPre> m_offlinePunishPre;
    std::map<SourceMM::PluginId, OnPlayerUnpunishCallback> m_unpunish;
    std::map<SourceMM::PluginId, OnOfflinePlayerUnpunishCallback> m_offlineUnpunish;
    std::map<SourceMM::PluginId, OnCoreLoadedCallback> m_coreLoadedCallbacks;
    std::map<SourceMM::PluginId, OnCategoryRegisterCallback> m_categoryRegister;
    std::map<SourceMM::PluginId, OnActionCallback> m_actions;
    bool m_coreLoaded = false;
};

extern VerdictApi* g_pApi;
