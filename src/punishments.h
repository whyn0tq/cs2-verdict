#pragma once

#include <string>

typedef unsigned long long uint64;

struct Punishment
{
    int expires;
    std::string reason;
    int adminId;
};

extern Punishment g_Punishments[64][4];

void ResetPunishments(int slot);
void CheckBan(int slot, uint64 steamId);
void LoadComms(int slot, uint64 steamId, bool announce);
void CheckExpiredPunishments();

bool IsPunished(int slot, int type);
bool IsVoiceBlocked(int slot);
bool IsChatBlocked(int slot);
int ActiveChatType(int slot);
int ActiveVoiceType(int slot);

bool PunishPlayer(int targetSlot, int type, int seconds, const std::string& reason, int adminSlot, bool notify, bool writeToDatabase);
void PunishOffline(uint64 steamId, const std::string& name, int type, int seconds, const std::string& reason, int adminSlot);
void PunishOfflineChecked(uint64 steamId, const std::string& name, int type, int seconds, const std::string& reason, int adminSlot, bool fromChat);
void BanIp(const std::string& ip, int seconds, const std::string& reason, int adminSlot);

bool CanRemovePunishment(int adminSlot, int targetSlot, int type);
bool UnpunishPlayer(int targetSlot, int type, int adminSlot, bool notify, const std::string& reason);
void UnpunishOffline(uint64 steamId, int type, int adminSlot, const std::string& reason, bool fromChat);
void UnbanIp(const std::string& ip, int adminSlot, const std::string& reason, bool fromChat);

void KickClient(int slot, const std::string& reason, bool banned);
void KickWithAnnounce(int targetSlot, int adminSlot, const std::string& reason);

std::string FormatDuration(int seconds);
std::string FormatTimeLeft(int expires);
const char* TypeKey(int type);
