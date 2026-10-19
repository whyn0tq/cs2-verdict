#pragma once

class CEntityInstance;

#define TEAM_NONE 0
#define TEAM_SPECTATOR 1
#define TEAM_T 2
#define TEAM_CT 3

CEntityInstance* ControllerOf(int slot);
CEntityInstance* PawnOf(int slot);
bool IsAlive(int slot);
int TeamOf(int slot);
int MoveTypeOf(int slot);

int HealthOf(int slot);
int ControllerIntOf(int slot, const char* fieldName);
int MatchStatOf(int slot, const char* fieldName);
int EntityTeam(CEntityInstance* entity);
int TeamScoreOf(CEntityInstance* team);
void SetHealth(int slot, int health);
void SetArmor(int slot, int armor);
void SetGravity(int slot, float scale);
void SetSpeed(int slot, float multiplier);
bool HasGodMode(int slot);
void SetGodMode(int slot, bool enabled);
bool OriginOf(int slot, float origin[3]);

void WriteEntityColor(CEntityInstance* entity, int red, int green, int blue, int alpha);
void SetPlayerColor(int slot, int red, int green, int blue);
