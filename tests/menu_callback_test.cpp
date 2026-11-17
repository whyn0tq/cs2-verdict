#include "menu_callback.h"

#include <cassert>
#include <cstdlib>
#include <iostream>

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

int main()
{
    TestCloseMenu();
    TestBack();
    std::cout << "2 menu callback tests passed\n";
}
