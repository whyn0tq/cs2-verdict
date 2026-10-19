#include "player_actions.h"
#include "admins.h"
#include "clients.h"
#include "effects.h"
#include "entities.h"
#include "phrases.h"
#include "punishments.h"
#include "settings.h"
#include "steam.h"
#include "common.h"
#include <cstdlib>

void AnnounceAction(int adminSlot, const std::string& message)
{
    META_CONPRINTF("[Verdict] %s\n", StripColors(message).c_str());
    if (g_Settings.notifyType == 2)
        ChatToAll(message);
    else if (IsValidSlot(adminSlot))
        ChatTo(adminSlot, message);
}

static std::string ActionText(const char* key, int adminSlot, int targetSlot)
{
    std::string text = Text(key);
    Replace(text, "{admin}", AdminDisplayName(adminSlot));
    Replace(text, "{target}", ClientName(targetSlot));
    return text;
}

std::string TeamName(int team)
{
    if (team == TEAM_T)
        return Text("Team_T");
    if (team == TEAM_CT)
        return Text("Team_CT");
    return Text("Team_Spectator");
}

bool SlayPlayer(int targetSlot, int adminSlot)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot))
        return false;

    g_pPlayers->CommitSuicide(targetSlot, false, true);
    AnnounceAction(adminSlot, ActionText("Action_Kill", adminSlot, targetSlot));
    return true;
}

bool SlapPlayer(int targetSlot, int adminSlot, int damage)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot))
        return false;

    Vector velocity((float)(rand() % 361 - 180), (float)(rand() % 361 - 180), (float)(250 + rand() % 100));
    g_pPlayers->Teleport(targetSlot, nullptr, nullptr, &velocity);
    PlaySoundAt(targetSlot, g_Settings.effects.slapSound);

    if (damage > 0)
    {
        int health = HealthOf(targetSlot) - damage;
        if (health <= 0)
            g_pPlayers->CommitSuicide(targetSlot, false, true);
        else
            SetHealth(targetSlot, health);
    }

    std::string text = ActionText("Action_Slap", adminSlot, targetSlot);
    Replace(text, "{damage}", std::to_string(damage));
    AnnounceAction(adminSlot, text);
    return true;
}

bool RenamePlayer(int targetSlot, int adminSlot, const std::string& name)
{
    std::string newName = CleanText(name, 64);
    if (!IsRealClient(targetSlot) || newName.empty())
        return false;

    std::string text = ActionText("Action_Rename", adminSlot, targetSlot);
    Replace(text, "{name}", newName);
    g_pPlayers->SetPlayerName(targetSlot, newName.c_str());
    AnnounceAction(adminSlot, text);
    return true;
}

bool MovePlayerToTeam(int targetSlot, int adminSlot, int team)
{
    if (!IsRealClient(targetSlot) || team < TEAM_SPECTATOR || team > TEAM_CT || TeamOf(targetSlot) == team)
        return false;

    if (team != TEAM_SPECTATOR && IsAlive(targetSlot))
        g_pPlayers->SwitchTeam(targetSlot, team);
    else
        g_pPlayers->ChangeTeam(targetSlot, team);

    std::string text = ActionText("Action_Team", adminSlot, targetSlot);
    Replace(text, "{team}", TeamName(team));
    AnnounceAction(adminSlot, text);
    return true;
}

bool ToggleNoclip(int targetSlot, int adminSlot)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot))
        return false;

    bool enable = MoveTypeOf(targetSlot) != MOVETYPE_NOCLIP;
    g_pPlayers->SetMoveType(targetSlot, enable ? MOVETYPE_NOCLIP : MOVETYPE_WALK);
    AnnounceAction(adminSlot, ActionText(enable ? "Action_NoclipOn" : "Action_NoclipOff", adminSlot, targetSlot));
    return true;
}

bool RespawnPlayer(int targetSlot, int adminSlot)
{
    int team = TeamOf(targetSlot);
    if (!IsRealClient(targetSlot) || IsAlive(targetSlot) || (team != TEAM_T && team != TEAM_CT))
        return false;

    g_pPlayers->Respawn(targetSlot);
    AnnounceAction(adminSlot, ActionText("Action_Respawn", adminSlot, targetSlot));
    return true;
}

struct PlayerEffects
{
    int burnSeconds;
    int freezeSeconds;
    float speed;
    int bombType;
    int bombTicks;
};

static PlayerEffects g_Effects[MAX_SLOTS];
static const int BurnDamagePerSecond = 5;

void ResetPlayerEffects(int slot)
{
    if (!IsValidSlot(slot))
        return;
    StopFireEffect(slot);
    g_Effects[slot].burnSeconds = 0;
    g_Effects[slot].freezeSeconds = 0;
    g_Effects[slot].speed = 1.0f;
    g_Effects[slot].bombType = BOMB_NONE;
    g_Effects[slot].bombTicks = 0;
}

std::string FormatMultiplier(float value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "x%g", value);
    return buffer;
}

static void StartBurning(int slot, int seconds)
{
    if (g_Effects[slot].burnSeconds <= 0)
    {
        StartFireEffect(slot);
        PlaySoundAt(slot, g_Settings.effects.igniteSound);
    }
    g_Effects[slot].burnSeconds = seconds;
}

static void StopBurning(int slot)
{
    g_Effects[slot].burnSeconds = 0;
    StopFireEffect(slot);
}

static void ApplyFreeze(int slot, int seconds)
{
    g_Effects[slot].freezeSeconds = seconds;
    g_pPlayers->SetMoveType(slot, MOVETYPE_OBSOLETE);
    SetPlayerColor(slot, 0, 128, 255);
    PlaySoundAt(slot, g_Settings.effects.freezeSound);
}

static void ReleaseFreeze(int slot)
{
    g_Effects[slot].freezeSeconds = 0;
    if (IsAlive(slot))
    {
        g_pPlayers->SetMoveType(slot, MOVETYPE_WALK);
        SetPlayerColor(slot, 255, 255, 255);
    }
}

static void BurnTick(int slot)
{
    PlayerEffects& effects = g_Effects[slot];
    if (effects.burnSeconds <= 0)
        return;
    if (!IsAlive(slot))
    {
        StopBurning(slot);
        return;
    }

    effects.burnSeconds--;
    int health = HealthOf(slot) - BurnDamagePerSecond;
    if (health <= 0)
    {
        StopBurning(slot);
        g_pPlayers->CommitSuicide(slot, false, true);
        return;
    }
    SetHealth(slot, health);
    if (effects.burnSeconds % 2 == 0)
        PlaySoundAt(slot, g_Settings.effects.burnSound);
    if (effects.burnSeconds <= 0)
        StopFireEffect(slot);
}

static void FreezeTick(int slot)
{
    PlayerEffects& effects = g_Effects[slot];
    if (effects.freezeSeconds <= 0)
        return;
    effects.freezeSeconds--;
    if (effects.freezeSeconds <= 0 || !IsAlive(slot))
        ReleaseFreeze(slot);
}

static void DetonateBomb(int slot)
{
    int type = g_Effects[slot].bombType;
    g_Effects[slot].bombType = BOMB_NONE;

    if (type == BOMB_TIME)
    {
        ShowExplosion(slot);
        PlaySoundAt(slot, g_Settings.effects.explosionSound);
        g_pPlayers->CommitSuicide(slot, true, true);
    }
    else if (type == BOMB_FIRE)
    {
        StartBurning(slot, g_Settings.effects.fireBombSeconds);
    }
    else if (type == BOMB_FREEZE)
    {
        ApplyFreeze(slot, g_Settings.effects.freezeBombSeconds);
    }

    std::string text = Text("Bomb_Detonated");
    Replace(text, "{target}", ClientName(slot));
    ChatToAll(text);
}

static void BombTick(int slot)
{
    PlayerEffects& effects = g_Effects[slot];
    if (effects.bombType == BOMB_NONE)
        return;
    if (!IsAlive(slot))
    {
        effects.bombType = BOMB_NONE;
        return;
    }

    effects.bombTicks--;
    if (effects.bombTicks <= 0)
    {
        DetonateBomb(slot);
        return;
    }

    PlaySoundAt(slot, effects.bombTicks <= 3 ? g_Settings.effects.bombLastTickSound : g_Settings.effects.bombTickSound);

    std::string countdown = Text("Bomb_Countdown");
    Replace(countdown, "{seconds}", std::to_string(effects.bombTicks));
    g_pUtils->PrintToCenter(slot, "%s", StripColors(countdown).c_str());
}

void TickPlayerEffects()
{
    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (!IsRealClient(slot))
            continue;

        BurnTick(slot);
        FreezeTick(slot);
        BombTick(slot);

        if (g_Effects[slot].speed != 1.0f && IsAlive(slot))
            SetSpeed(slot, g_Effects[slot].speed);
    }
}

bool BurnPlayer(int targetSlot, int adminSlot, int seconds)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot) || seconds <= 0)
        return false;

    StartBurning(targetSlot, seconds);
    std::string text = ActionText("Action_Burn", adminSlot, targetSlot);
    Replace(text, "{seconds}", std::to_string(seconds));
    AnnounceAction(adminSlot, text);
    return true;
}

bool FreezePlayer(int targetSlot, int adminSlot, int seconds)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot))
        return false;

    if (seconds <= 0)
    {
        ReleaseFreeze(targetSlot);
        AnnounceAction(adminSlot, ActionText("Action_FreezeOff", adminSlot, targetSlot));
        return true;
    }

    ApplyFreeze(targetSlot, seconds);
    std::string text = ActionText("Action_FreezeOn", adminSlot, targetSlot);
    Replace(text, "{seconds}", std::to_string(seconds));
    AnnounceAction(adminSlot, text);
    return true;
}

bool IsFrozen(int slot)
{
    return IsValidSlot(slot) && g_Effects[slot].freezeSeconds > 0;
}

bool ToggleBomb(int targetSlot, int adminSlot, int type)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot) || type < BOMB_TIME || type > BOMB_FREEZE)
        return false;

    static const char* startKeys[] = { "", "Action_TimeBombOn", "Action_FireBombOn", "Action_FreezeBombOn" };
    PlayerEffects& effects = g_Effects[targetSlot];
    if (effects.bombType == type)
    {
        effects.bombType = BOMB_NONE;
        AnnounceAction(adminSlot, ActionText("Action_BombOff", adminSlot, targetSlot));
        return true;
    }

    effects.bombType = type;
    effects.bombTicks = g_Settings.effects.bombTicks + 1;
    std::string text = ActionText(startKeys[type], adminSlot, targetSlot);
    Replace(text, "{seconds}", std::to_string(g_Settings.effects.bombTicks));
    AnnounceAction(adminSlot, text);
    return true;
}

bool SetPlayerHealth(int targetSlot, int adminSlot, int health)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot) || health <= 0)
        return false;

    SetHealth(targetSlot, health);
    std::string text = ActionText("Action_Health", adminSlot, targetSlot);
    Replace(text, "{value}", std::to_string(health));
    AnnounceAction(adminSlot, text);
    return true;
}

bool SetPlayerArmor(int targetSlot, int adminSlot, int armor)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot) || armor < 0)
        return false;

    SetArmor(targetSlot, armor);
    std::string text = ActionText("Action_Armor", adminSlot, targetSlot);
    Replace(text, "{value}", std::to_string(armor));
    AnnounceAction(adminSlot, text);
    return true;
}

bool SetPlayerGravity(int targetSlot, int adminSlot, float scale)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot) || scale <= 0.0f || scale > 10.0f)
        return false;

    SetGravity(targetSlot, scale);
    std::string text = ActionText("Action_Gravity", adminSlot, targetSlot);
    Replace(text, "{value}", FormatMultiplier(scale));
    AnnounceAction(adminSlot, text);
    return true;
}

bool SetPlayerSpeed(int targetSlot, int adminSlot, float multiplier)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot) || multiplier <= 0.0f || multiplier > 10.0f)
        return false;

    g_Effects[targetSlot].speed = multiplier;
    SetSpeed(targetSlot, multiplier);
    std::string text = ActionText("Action_Speed", adminSlot, targetSlot);
    Replace(text, "{value}", FormatMultiplier(multiplier));
    AnnounceAction(adminSlot, text);
    return true;
}

bool ToggleGodMode(int targetSlot, int adminSlot)
{
    if (!IsRealClient(targetSlot) || !IsAlive(targetSlot))
        return false;

    bool enable = !HasGodMode(targetSlot);
    SetGodMode(targetSlot, enable);
    AnnounceAction(adminSlot, ActionText(enable ? "Action_GodOn" : "Action_GodOff", adminSlot, targetSlot));
    return true;
}

static std::string FlagLetters(int slot)
{
    std::string letters;
    for (size_t i = 0; i < g_Admins[slot].flags.size(); i++)
        letters += g_Admins[slot].flags[i];
    return letters;
}

void ShowPlayerInfo(int viewerSlot, bool fromChat, int targetSlot)
{
    if (!IsRealClient(targetSlot))
        return;

    uint64 steamId = ClientSteamId(targetSlot);
    std::string header = Text("Who_Header");
    Replace(header, "{name}", ClientName(targetSlot));
    Replace(header, "{team}", TeamName(TeamOf(targetSlot)));
    ReplyTo(viewerSlot, fromChat, header);

    std::string ids = Text("Who_SteamId");
    Replace(ids, "{steam2}", SteamIdToSteam2(steamId));
    Replace(ids, "{steam64}", SteamIdToText(steamId));
    ReplyTo(viewerSlot, fromChat, ids);

    if (IsAdmin(targetSlot))
    {
        std::string admin = Text("Who_Admin");
        Replace(admin, "{group}", g_Admins[targetSlot].groupName.empty() ? "-" : g_Admins[targetSlot].groupName);
        Replace(admin, "{flags}", FlagLetters(targetSlot));
        Replace(admin, "{immunity}", std::to_string(g_Admins[targetSlot].immunity));
        ReplyTo(viewerSlot, fromChat, admin);
    }

    bool punished = false;
    for (int type = RT_MUTE; type <= RT_SILENCE; type++)
    {
        if (!IsPunished(targetSlot, type))
            continue;
        punished = true;

        std::string line = Text("Who_Punishment");
        Replace(line, "{type}", Text((std::string("Type_") + TypeKey(type)).c_str()));
        Replace(line, "{time}", FormatTimeLeft(g_Punishments[targetSlot][type].expires));
        Replace(line, "{reason}", g_Punishments[targetSlot][type].reason);
        ReplyTo(viewerSlot, fromChat, line);
    }
    if (!punished)
        ReplyTo(viewerSlot, fromChat, Text("Who_Clean"));
}
