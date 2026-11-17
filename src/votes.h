#pragma once

#include <string>

#define VOTE_KICK 0
#define VOTE_BAN 1
#define VOTE_MUTE 2
#define VOTE_MAP 3

void RegisterVoteMenuItems();
bool StartPlayerVote(int adminSlot, int type, int targetSlot);
bool StartMapVote(int adminSlot, const std::string& map);
bool IsVoteRunning();
