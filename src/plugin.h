#pragma once

#include "common.h"

class Verdict final : public ISmmPlugin, public IMetamodListener, public IPlayerListener
{
public:
    bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late) override;
    bool Unload(char* error, size_t maxlen) override;
    void AllPluginsLoaded() override;
    void* OnMetamodQuery(const char* iface, int* ret) override;
    void OnPluginUnload(PluginId id) override;

    void OnClientAuthorized(int iSlot, uint64 iSteamID64) override;
    void ClientDisconnect(int iSlot) override;

    const char* GetAuthor() override;
    const char* GetName() override;
    const char* GetDescription() override;
    const char* GetURL() override;
    const char* GetLicense() override;
    const char* GetVersion() override;
    const char* GetDate() override;
    const char* GetLogTag() override;
};

void LoadClient(int slot, uint64 steamId);
void SyncPlayer(int slot);
