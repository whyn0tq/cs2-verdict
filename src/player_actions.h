#pragma once

#include <string>

#define BOMB_NONE 0
#define BOMB_TIME 1
#define BOMB_FIRE 2
#define BOMB_FREEZE 3

void AnnounceAction(int adminSlot, const std::string& message);
std::string TeamName(int team);

bool SlayPlayer(int targetSlot, int adminSlot);
bool SlapPlayer(int targetSlot, int adminSlot, int damage);
bool RenamePlayer(int targetSlot, int adminSlot, const std::string& name);
bool MovePlayerToTeam(int targetSlot, int adminSlot, int team);
bool ToggleNoclip(int targetSlot, int adminSlot);
bool FreezePlayer(int targetSlot, int adminSlot, int seconds);
bool IsFrozen(int slot);
bool ToggleBomb(int targetSlot, int adminSlot, int type);
bool RespawnPlayer(int targetSlot, int adminSlot);
void ShowPlayerInfo(int viewerSlot, bool fromChat, int targetSlot);

bool BurnPlayer(int targetSlot, int adminSlot, int seconds);
bool SetPlayerHealth(int targetSlot, int adminSlot, int health);
bool SetPlayerArmor(int targetSlot, int adminSlot, int armor);
bool SetPlayerGravity(int targetSlot, int adminSlot, float scale);
bool SetPlayerSpeed(int targetSlot, int adminSlot, float multiplier);
bool ToggleGodMode(int targetSlot, int adminSlot);

std::string FormatMultiplier(float value);
void ResetPlayerEffects(int slot);
void TickPlayerEffects();
