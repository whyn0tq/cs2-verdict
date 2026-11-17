#pragma once

#define MENU_OWNER_INTERFACE "MenuOwnerApi001"

struct Menu;

class IMenuOwnerApi
{
public:
    virtual void DisplayMenu(int owner, Menu& menu, int slot) = 0;
    virtual void CloseMenus(int owner) = 0;
};
