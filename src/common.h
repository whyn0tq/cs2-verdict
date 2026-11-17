#pragma once

#include <ISmmPlugin.h>
#include "utlvector.h"
#include "ehandle.h"
#include <engine2/isource2server.h>
#include <engine2/ivengineserver2.h>
#include <entity2/entitysystem.h>
#include "igameevents.h"
#include "vector.h"
#include <utlstring.h>
#include <keyvalues.h>
#include "menus.h"
#include "utils.h"
#include "players.h"
#include "verdict.h"

#include <string>
#include <vector>
#include <map>
#include <ctime>

#define MAX_SLOTS 64
#define CONSOLE_SLOT -1

PLUGIN_GLOBALVARS();

extern IVEngineServer2* engine;
extern IUtilsApi* g_pUtils;
extern IMenusApi* g_pMenus;
extern IPlayersApi* g_pPlayers;

inline bool IsValidSlot(int slot)
{
    return slot >= 0 && slot < MAX_SLOTS;
}

inline int Now()
{
    return (int)std::time(nullptr);
}
