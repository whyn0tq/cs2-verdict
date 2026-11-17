#pragma once

#include <string>

std::string MapTitle(const std::string& map);
void ChangeMap(int adminSlot, const std::string& map);
void RestartGame(int adminSlot);
void SwapTeams(int adminSlot);
void ExecuteConfig(int adminSlot, const std::string& file);
void ReloadEveryone(int adminSlot);
void RunServerCommand(int adminSlot, const std::string& command);
