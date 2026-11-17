#pragma once

#include <ISmmPlugin.h>
#include <string>
#include "admin.h"
#include "menus.h"

void SetMenuCallback(Menu& menu, MenuCallbackFunc callback);
void DisplayMenu(Menu& menu, int slot);
void RegisterMenuCategory(const char* identity, const char* name, OnCategoryDisplayCallback callback);
void RegisterMenuItem(const char* identity, const char* name, const char* category, const char* permissions, OnItemDisplayCallback onDisplay, OnItemSelectCallback onSelect);
void RegisterCoreMenuItems();

bool ShowAdminMenu(int slot);
void ShowCategory(int slot, const std::string& category);
void ShowLastCategory(int slot);
void ShowItem(int slot, const std::string& category, const std::string& identity);

bool HandleOwnReason(int slot, const std::string& text);
void ResetMenuState(int slot);
