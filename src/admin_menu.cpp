#include "admin_menu.h"
#include "admins.h"
#include "api.h"
#include "clients.h"
#include "phrases.h"
#include "punishments.h"
#include "settings.h"
#include "steam.h"
#include "storage.h"
#include "common.h"
#include <algorithm>

struct MenuItemInfo
{
    std::string identity;
    std::string name;
    std::string category;
    std::vector<std::string> permissions;
    OnItemDisplayCallback onDisplay;
    OnItemSelectCallback onSelect;
};

struct MenuCategoryInfo
{
    std::string identity;
    std::string name;
    std::vector<std::string> order;
    std::map<std::string, MenuItemInfo> items;
    OnCategoryDisplayCallback onDisplay;
};

struct PunishDraft
{
    int type;
    bool offline;
    int targetSlot;
    uint64 targetSteamId;
    std::string targetName;
    std::string reasonKey;
    std::string reason;
    bool waitingForReason;
};

static std::map<std::string, MenuCategoryInfo> g_Categories;
static std::vector<std::string> g_CategoryOrder;
static std::string g_LastCategory[MAX_SLOTS];
static PunishDraft g_Drafts[MAX_SLOTS];

static const char* g_PunishItemKeys[4] = { "Item_BanPlayer", "Item_MutePlayer", "Item_GagPlayer", "Item_SilencePlayer" };
static const char* g_UnpunishItemKeys[4] = { "Item_UnBanPlayer", "Item_UnMutePlayer", "Item_UnGagPlayer", "Item_UnSilencePlayer" };
static const char* g_PunishPermissions[4] = { "@admin/ban", "@admin/mute", "@admin/gag", "@admin/silence" };
static const char* g_UnpunishPermissions[4] = { "@admin/unban", "@admin/unmute", "@admin/ungag", "@admin/unsilence" };

static void ShowPunishTypes(int slot, bool offline);
static void ShowUnpunishTypes(int slot, bool offline);
static void ShowReasons(int slot);
static void ShowTimes(int slot);

static std::vector<std::string> SplitPermissions(const std::string& text)
{
    std::vector<std::string> result;
    size_t start = 0;
    while (start <= text.length())
    {
        size_t end = text.find('|', start);
        if (end == std::string::npos)
            end = text.length();
        result.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return result;
}

static MenuCategoryInfo& CategoryFor(const std::string& identity)
{
    if (g_Categories.find(identity) == g_Categories.end())
        g_CategoryOrder.push_back(identity);
    return g_Categories[identity];
}

void RegisterMenuCategory(const char* identity, const char* name, OnCategoryDisplayCallback callback)
{
    if (!identity)
        return;
    g_pApi->NotifyCategoryRegistered(identity);

    MenuCategoryInfo& category = CategoryFor(identity);
    category.identity = identity;
    category.name = name ? name : identity;
    category.onDisplay = callback;
}

void RegisterMenuItem(const char* identity, const char* name, const char* category, const char* permissions, OnItemDisplayCallback onDisplay, OnItemSelectCallback onSelect)
{
    if (!identity || !category)
        return;

    MenuCategoryInfo& owner = CategoryFor(category);
    if (owner.items.find(identity) == owner.items.end())
        owner.order.push_back(identity);

    MenuItemInfo& item = owner.items[identity];
    item.identity = identity;
    item.name = name ? name : identity;
    item.category = category;
    item.permissions = SplitPermissions(permissions ? permissions : "");
    item.onDisplay = onDisplay;
    item.onSelect = onSelect;
}

static bool CanSeeItem(int slot, const MenuItemInfo& item)
{
    if (!IsAdmin(slot))
        return false;
    for (size_t i = 0; i < item.permissions.size(); i++)
    {
        if (HasPermission(slot, item.permissions[i].c_str()))
            return true;
    }
    return false;
}

static bool CanSeeCategory(int slot, const MenuCategoryInfo& category)
{
    if (category.identity.empty())
        return false;
    std::map<std::string, MenuItemInfo>::const_iterator it;
    for (it = category.items.begin(); it != category.items.end(); ++it)
    {
        if (CanSeeItem(slot, it->second))
            return true;
    }
    return false;
}

static std::vector<std::string> SortedList(const std::vector<std::string>& preferred, const std::vector<std::string>& registered)
{
    std::vector<std::string> result;
    for (size_t i = 0; i < preferred.size(); i++)
    {
        bool known = std::find(registered.begin(), registered.end(), preferred[i]) != registered.end();
        bool added = std::find(result.begin(), result.end(), preferred[i]) != result.end();
        if (known && !added)
            result.push_back(preferred[i]);
    }
    for (size_t i = 0; i < registered.size(); i++)
    {
        if (std::find(result.begin(), result.end(), registered[i]) == result.end())
            result.push_back(registered[i]);
    }
    return result;
}

bool ShowAdminMenu(int slot)
{
    if (!IsValidSlot(slot) || !IsAdmin(slot))
        return true;

    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase("MainTitle"));

    std::vector<std::string> order = SortedList(g_Settings.categoryOrder, g_CategoryOrder);
    bool anything = false;
    for (size_t i = 0; i < order.size(); i++)
    {
        MenuCategoryInfo& category = g_Categories[order[i]];
        if (!CanSeeCategory(slot, category))
            continue;

        std::string title = Phrase(category.name.c_str());
        if (category.onDisplay)
            category.onDisplay(slot, category.identity.c_str(), title);
        g_pMenus->AddItemMenu(menu, category.identity.c_str(), title.c_str());
        anything = true;
    }

    if (!anything)
        g_pMenus->AddItemMenu(menu, "", Phrase("NoAccess"), ITEM_DISABLED);

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, false);
    g_pMenus->SetCallback(menu, [](const char* back, const char* front, int item, int slot) {
        if (item >= 7 || !back || !back[0])
            return;
        g_LastCategory[slot] = back;
        ShowCategory(slot, back);
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
    return true;
}

void ShowCategory(int slot, const std::string& identity)
{
    std::map<std::string, MenuCategoryInfo>::iterator found = g_Categories.find(identity);
    if (found == g_Categories.end() || !IsValidSlot(slot))
        return;

    MenuCategoryInfo& category = found->second;
    g_LastCategory[slot] = identity;

    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase(category.name.c_str()));

    std::vector<std::string> preferred;
    std::map<std::string, std::vector<std::string>>::iterator sorting = g_Settings.itemOrder.find(identity);
    if (sorting != g_Settings.itemOrder.end())
        preferred = sorting->second;

    std::vector<std::string> order = SortedList(preferred, category.order);
    for (size_t i = 0; i < order.size(); i++)
    {
        MenuItemInfo& item = category.items[order[i]];
        if (!CanSeeItem(slot, item))
            continue;

        std::string title = Phrase(item.name.c_str());
        if (item.onDisplay)
            item.onDisplay(slot, item.category.c_str(), item.identity.c_str(), title);
        g_pMenus->AddItemMenu(menu, item.identity.c_str(), title.c_str());
    }

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [](const char* back, const char* front, int item, int slot) {
        if (item < 7)
        {
            if (back && back[0])
                ShowItem(slot, g_LastCategory[slot], back);
        }
        else if (item == 7)
        {
            ShowAdminMenu(slot);
        }
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

void ShowLastCategory(int slot)
{
    if (!IsValidSlot(slot))
        return;
    if (g_LastCategory[slot].empty())
        ShowAdminMenu(slot);
    else
        ShowCategory(slot, g_LastCategory[slot]);
}

void ShowItem(int slot, const std::string& category, const std::string& identity)
{
    std::map<std::string, MenuCategoryInfo>::iterator owner = g_Categories.find(category);
    if (owner == g_Categories.end())
        return;

    std::map<std::string, MenuItemInfo>::iterator found = owner->second.items.find(identity);
    if (found == owner->second.items.end())
        return;

    MenuItemInfo& item = found->second;
    if (item.onSelect && CanSeeItem(slot, item))
        item.onSelect(slot, item.category.c_str(), item.identity.c_str(), item.name.c_str());
}

void ResetMenuState(int slot)
{
    if (!IsValidSlot(slot))
        return;
    g_LastCategory[slot].clear();
    g_Drafts[slot] = PunishDraft();
    g_Drafts[slot].waitingForReason = false;
}

static void AddNothingFound(Menu& menu, const char* key)
{
    g_pMenus->AddItemMenu(menu, "", Phrase(key), ITEM_DISABLED);
}

static void ShowPunishTargets(int slot)
{
    PunishDraft& draft = g_Drafts[slot];

    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase("SelectPlayer"));

    bool found = false;
    for (int target = 0; target < MAX_SLOTS; target++)
    {
        if (!IsRealClient(target) || IsPunished(target, draft.type))
            continue;
        if (!CanTarget(slot, target))
            continue;
        if (target == slot && g_Settings.immunityType != 0)
            continue;

        std::string name = g_pMenus->escapeString(ClientName(target));
        g_pMenus->AddItemMenu(menu, std::to_string(target).c_str(), name.c_str());
        found = true;
    }
    if (!found)
        AddNothingFound(menu, "NoPlayers");

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [](const char* back, const char* front, int item, int slot) {
        if (item < 7)
        {
            int target = atoi(back);
            if (!IsRealClient(target))
                return;
            PunishDraft& draft = g_Drafts[slot];
            draft.targetSlot = target;
            draft.targetSteamId = ClientSteamId(target);
            draft.targetName = ClientName(target);
            ShowReasons(slot);
        }
        else if (item == 7)
        {
            ShowPunishTypes(slot, false);
        }
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static void ShowOfflineTargets(int slot)
{
    PunishDraft& draft = g_Drafts[slot];

    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase("SelectOfflinePlayer"));

    bool found = false;
    for (size_t i = g_RecentClients.size(); i > 0; i--)
    {
        RecentClient& recent = g_RecentClients[i - 1];
        if (recent.punished[draft.type])
            continue;

        std::string name = g_pMenus->escapeString(recent.name);
        g_pMenus->AddItemMenu(menu, SteamIdToText(recent.steamId).c_str(), name.c_str());
        found = true;
    }
    if (!found)
        AddNothingFound(menu, "NoPlayers");

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [](const char* back, const char* front, int item, int slot) {
        if (item < 7)
        {
            uint64 steamId = 0;
            if (!ParseSteamId(back, steamId))
                return;
            PunishDraft& draft = g_Drafts[slot];
            draft.targetSlot = -1;
            draft.targetSteamId = steamId;
            RecentClient* recent = FindRecentClient(steamId);
            draft.targetName = recent ? recent->name : "";
            ShowReasons(slot);
        }
        else if (item == 7)
        {
            ShowPunishTypes(slot, true);
        }
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static void ShowPunishTypes(int slot, bool offline)
{
    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase("SelectPunishType"));
    for (int type = RT_BAN; type <= RT_SILENCE; type++)
    {
        if (HasPermission(slot, g_PunishPermissions[type]))
            g_pMenus->AddItemMenu(menu, std::to_string(type).c_str(), Phrase(g_PunishItemKeys[type]));
    }

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [offline](const char* back, const char* front, int item, int slot) {
        if (item < 7)
        {
            PunishDraft& draft = g_Drafts[slot];
            draft = PunishDraft();
            draft.waitingForReason = false;
            draft.type = atoi(back);
            draft.offline = offline;
            if (offline)
                ShowOfflineTargets(slot);
            else
                ShowPunishTargets(slot);
        }
        else if (item == 7)
        {
            ShowLastCategory(slot);
        }
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static bool OwnReasonAllowed(int slot)
{
    return g_Settings.ownReasons && HasPermission(slot, "@admin/own_reason");
}

static void ShowReasons(int slot)
{
    PunishDraft& draft = g_Drafts[slot];

    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase("SelectReason"));

    if (g_Settings.timeReasonType == 1)
    {
        std::vector<ReasonOption>& reasons = g_Settings.reasonTimes[draft.type];
        for (size_t i = 0; i < reasons.size(); i++)
        {
            if (reasons[i].reason == "own")
            {
                if (OwnReasonAllowed(slot))
                    g_pMenus->AddItemMenu(menu, "own", Phrase("Item_OwnReason"));
                continue;
            }
            g_pMenus->AddItemMenu(menu, reasons[i].reason.c_str(), reasons[i].reason.c_str());
        }
    }
    else
    {
        if (OwnReasonAllowed(slot))
            g_pMenus->AddItemMenu(menu, "own", Phrase("Item_OwnReason"));
        std::vector<std::string>& reasons = g_Settings.reasons[draft.type];
        for (size_t i = 0; i < reasons.size(); i++)
            g_pMenus->AddItemMenu(menu, reasons[i].c_str(), reasons[i].c_str());
    }

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [](const char* back, const char* front, int item, int slot) {
        PunishDraft& draft = g_Drafts[slot];
        if (item < 7)
        {
            draft.reasonKey = back;
            if (draft.reasonKey == "own")
            {
                draft.waitingForReason = true;
                g_pMenus->ClosePlayerMenu(slot);
                ChatTo(slot, Text("EnterOwnReason"));
                return;
            }
            draft.reason = back;
            ShowTimes(slot);
        }
        else if (item == 7)
        {
            if (draft.offline)
                ShowOfflineTargets(slot);
            else
                ShowPunishTargets(slot);
        }
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static std::vector<TimeOption> TimesForDraft(const PunishDraft& draft)
{
    if (g_Settings.timeReasonType != 1)
        return g_Settings.times[draft.type];

    std::vector<ReasonOption>& reasons = g_Settings.reasonTimes[draft.type];
    for (size_t i = 0; i < reasons.size(); i++)
    {
        if (reasons[i].reason == draft.reasonKey)
            return reasons[i].times;
    }
    return std::vector<TimeOption>();
}

static void ApplyDraft(int slot, int seconds)
{
    PunishDraft& draft = g_Drafts[slot];
    if (!CanUseDuration(slot, draft.type, seconds))
    {
        ChatTo(slot, Text("TimeNotAllowed"));
        return;
    }

    if (draft.offline)
    {
        PunishOfflineChecked(draft.targetSteamId, draft.targetName, draft.type, seconds, draft.reason, slot, true);
        return;
    }

    if (!IsSameClient(draft.targetSlot, draft.targetSteamId))
    {
        ChatTo(slot, Text("PlayerLeft"));
        return;
    }
    if (!CanTarget(slot, draft.targetSlot))
    {
        ChatTo(slot, Text("TargetImmune"));
        return;
    }
    PunishPlayer(draft.targetSlot, draft.type, seconds, draft.reason, slot, true, true);
}

static void ShowTimes(int slot)
{
    PunishDraft& draft = g_Drafts[slot];

    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase("SelectTime"));

    std::vector<TimeOption> times = TimesForDraft(draft);
    bool found = false;
    for (size_t i = 0; i < times.size(); i++)
    {
        if (!CanUseDuration(slot, draft.type, times[i].seconds))
            continue;
        g_pMenus->AddItemMenu(menu, std::to_string(times[i].seconds).c_str(), times[i].title.c_str());
        found = true;
    }
    if (!found)
        AddNothingFound(menu, "NoTimes");

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [](const char* back, const char* front, int item, int slot) {
        if (item < 7)
        {
            g_pMenus->ClosePlayerMenu(slot);
            ApplyDraft(slot, atoi(back));
        }
        else if (item == 7)
        {
            ShowReasons(slot);
        }
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

bool HandleOwnReason(int slot, const std::string& text)
{
    if (!IsValidSlot(slot) || !g_Drafts[slot].waitingForReason)
        return false;

    PunishDraft& draft = g_Drafts[slot];
    draft.waitingForReason = false;

    std::string reason = CleanText(text, 250);
    if (reason.empty())
    {
        ChatTo(slot, Text("EnterOwnReason"));
        draft.waitingForReason = true;
        return true;
    }

    draft.reason = reason;
    ShowTimes(slot);
    return true;
}

static void ShowUnpunishTargets(int slot, int type)
{
    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase("SelectPlayer"));

    bool found = false;
    for (int target = 0; target < MAX_SLOTS; target++)
    {
        if (!IsRealClient(target) || !CanRemovePunishment(slot, target, type))
            continue;
        std::string name = g_pMenus->escapeString(ClientName(target));
        g_pMenus->AddItemMenu(menu, std::to_string(target).c_str(), name.c_str());
        found = true;
    }
    if (!found)
        AddNothingFound(menu, "NoPlayers");

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [type](const char* back, const char* front, int item, int slot) {
        if (item < 7)
        {
            int target = atoi(back);
            g_pMenus->ClosePlayerMenu(slot);
            if (UnpunishPlayer(target, type, slot, true, ""))
            {
                std::string message = Text("UnPunishSuccess");
                Replace(message, "{target}", front ? front : "");
                ChatTo(slot, message);
            }
            else
            {
                ChatTo(slot, Text("NothingToRemove"));
            }
        }
        else if (item == 7)
        {
            ShowUnpunishTypes(slot, false);
        }
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static std::string OfflineUnpunishQuery(int slot, int type)
{
    std::string restriction;
    bool everything = g_Settings.unpunishType == 1 || HasPermission(slot, "@admin/unpunish_all");
    if (!everything)
        restriction = " AND `aid` = " + std::to_string(AdminDatabaseId(slot));

    std::string active = " AND (`length` = 0 OR `ends` > UNIX_TIMESTAMP()) AND `RemoveType` IS NULL";
    std::string limit = " ORDER BY `bid` DESC LIMIT " + std::to_string(g_Settings.offlineUnpunishCount);

    if (type == RT_BAN)
        return "SELECT `authid`, `name` FROM " + Table("bans") + " WHERE `type` = 0 AND `authid` != ''" + active + restriction + limit;

    std::string types = "1, 2, 3";
    if (type == RT_MUTE)
        types = "1, 3";
    else if (type == RT_GAG)
        types = "2, 3";
    return "SELECT `authid`, `name` FROM " + Table("comms") + " WHERE `type` IN (" + types + ")" + active + restriction + limit;
}

static void ShowOfflineUnpunishTargets(int slot, int type)
{
    uint64 adminSteamId = ClientSteamId(slot);
    RunQuery(OfflineUnpunishQuery(slot, type), [slot, type, adminSteamId](ISQLQuery* query) {
        ISQLResult* result = ResultOf(query);
        if (!result || !IsSameClient(slot, adminSteamId))
            return;

        Menu menu;
        g_pMenus->SetTitleMenu(menu, Phrase("SelectOfflinePlayer"));

        std::vector<std::string> seen;
        while (result->FetchRow())
        {
            std::string authid = ReadString(result, 0);
            if (std::find(seen.begin(), seen.end(), authid) != seen.end())
                continue;
            seen.push_back(authid);

            std::string title = g_pMenus->escapeString(ReadString(result, 1)) + " (" + authid + ")";
            g_pMenus->AddItemMenu(menu, authid.c_str(), title.c_str());
        }
        if (seen.empty())
            AddNothingFound(menu, "NoPlayers");

        g_pMenus->SetExitMenu(menu, true);
        g_pMenus->SetBackMenu(menu, true);
        g_pMenus->SetCallback(menu, [type](const char* back, const char* front, int item, int slot) {
            if (item < 7)
            {
                uint64 steamId = 0;
                g_pMenus->ClosePlayerMenu(slot);
                if (ParseSteamId(back, steamId))
                    UnpunishOffline(steamId, type, slot, "", true);
            }
            else if (item == 7)
            {
                ShowUnpunishTypes(slot, true);
            }
        });
        g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
    });
}

static void ShowUnpunishTypes(int slot, bool offline)
{
    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase("SelectUnPunishType"));
    for (int type = RT_BAN; type <= RT_SILENCE; type++)
    {
        if (type == RT_BAN && !offline)
            continue;
        if (HasPermission(slot, g_UnpunishPermissions[type]))
            g_pMenus->AddItemMenu(menu, std::to_string(type).c_str(), Phrase(g_UnpunishItemKeys[type]));
    }

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [offline](const char* back, const char* front, int item, int slot) {
        if (item < 7)
        {
            if (offline)
                ShowOfflineUnpunishTargets(slot, atoi(back));
            else
                ShowUnpunishTargets(slot, atoi(back));
        }
        else if (item == 7)
        {
            ShowLastCategory(slot);
        }
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

void RegisterCoreMenuItems()
{
    RegisterMenuCategory("punishments", "Category_Punishments", nullptr);

    RegisterMenuItem("punish", "Item_PunishPlayer", "punishments", "@admin/ban|@admin/mute|@admin/gag|@admin/silence", nullptr,
        [](int slot, const char*, const char*, const char*) { ShowPunishTypes(slot, false); });

    RegisterMenuItem("unpunish", "Item_UnPunishPlayer", "punishments", "@admin/unmute|@admin/ungag|@admin/unsilence", nullptr,
        [](int slot, const char*, const char*, const char*) { ShowUnpunishTypes(slot, false); });

    RegisterMenuItem("punish_offline", "Item_PunishOfflinePlayer", "punishments", "@admin/ban|@admin/mute|@admin/gag|@admin/silence", nullptr,
        [](int slot, const char*, const char*, const char*) { ShowPunishTypes(slot, true); });

    RegisterMenuItem("unpunish_offline", "Item_UnPunishOfflinePlayer", "punishments", "@admin/unban|@admin/unmute|@admin/ungag|@admin/unsilence", nullptr,
        [](int slot, const char*, const char*, const char*) { ShowUnpunishTypes(slot, true); });
}
