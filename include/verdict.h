#pragma once

#include "admin.h"

#define VERDICT_INTERFACE "IVerdictApi"

class IVerdictApi : public IAdminApi
{
public:
    virtual int GetServerID() = 0;
    virtual const char* GetTablePrefix() = 0;
    virtual int GetAdminDatabaseID(int iSlot) = 0;
    virtual void ReloadPlayer(int iSlot) = 0;
    virtual void KickPlayer(int iSlot, const char* szReason, int iAdminID = -1) = 0;
};
