#pragma once

#include <string>

void CreatePanelTables();
void PanelLinkTick();
void PanelPlayerJoined(int slot);
void PanelPlayerLeft(int slot);
void PanelChatMessage(int slot, const std::string& text, bool teamOnly, bool blocked);
