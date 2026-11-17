#pragma once

#include <string>
#include <vector>

void AddPanelTables(std::vector<std::string>& queries);
void PanelLinkTick();
void PanelPlayerJoined(int slot);
void PanelPlayerLeft(int slot);
void PanelChatMessage(int slot, const std::string& text, bool teamOnly, bool blocked);
