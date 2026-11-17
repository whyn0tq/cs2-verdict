#pragma once

#include <utility>
#include "menus.h"

inline MenuCallbackFunc WrapMenuCallback(MenuCallbackFunc callback)
{
    return [callback = std::move(callback)](const char* back, const char* front, int item, int slot) {
        auto run = callback;
        std::string value = back ? back : "";
        std::string label = front ? front : "";
        run(back ? value.c_str() : nullptr, front ? label.c_str() : nullptr, item, slot);
    };
}
