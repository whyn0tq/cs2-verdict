#include "clients.h"
#include "menu_callback.h"

#include <cassert>
#include <cstdlib>
#include <iostream>

static uint64 g_SteamId = 76561198000000000ULL;

uint64 ClientSteamId(int slot)
{
    return slot == 3 ? g_SteamId : 0;
}

bool IsSameClient(int slot, uint64 steamId)
{
    return steamId != 0 && ClientSteamId(slot) == steamId;
}

static void TestCloseMenu()
{
    Menu menu;
    menu.hItems.push_back({ITEM_DEFAULT, std::string(64, '0') + "300", std::string(64, 'x')});
    int seconds = -1;
    menu.hFunc = WrapMenuCallback([&menu, &seconds, expected = std::string(64, 'x')](const char* back, const char* front, int item, int slot) {
        menu.clear();
        assert(item == 1 && slot == 2);
        assert(expected == front);
        seconds = std::atoi(back);
    });
    menu.hFunc(menu.hItems[0].sBack.c_str(), menu.hItems[0].sText.c_str(), 1, 2);
    assert(menu.hItems.empty() && !menu.hFunc);
    assert(seconds == 300);
}

static void TestBack()
{
    bool called = false;
    auto callback = WrapMenuCallback([&called](const char* back, const char* front, int item, int slot) {
        assert(back == nullptr && front == nullptr);
        assert(item == 7 && slot == 2);
        called = true;
    });
    callback(nullptr, nullptr, 7, 2);
    assert(called);
}

static void TestTarget()
{
    const std::string key = ClientKey(3);
    assert(ClientFromKey(key) == 3);
    assert(ClientFromKey("") == -1);
    assert(ClientFromKey("3") == -1);
    assert(ClientFromKey("3:0") == -1);
    assert(ClientFromKey(key + "suffix") == -1);
    assert(ClientFromKey("999999999999:76561198000000000") == -1);

    g_SteamId = 0;
    assert(ClientFromKey(key) == -1);
    g_SteamId = 76561198000000001ULL;
    assert(ClientFromKey(key) == -1);
    assert(ClientFromKey(ClientKey(3)) == 3);
}

int main()
{
    TestCloseMenu();
    TestBack();
    TestTarget();
    std::cout << "3 menu tests passed\n";
}
