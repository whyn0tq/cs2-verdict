#pragma once

#include <string>

void RegisterManagementMenuItems();
bool HandleMenuTextInput(int slot, const std::string& text);
void ResetManagementState(int slot);
