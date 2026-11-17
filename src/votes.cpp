#include "votes.h"
#include "admin_menu.h"
#include "admins.h"
#include "clients.h"
#include "phrases.h"
#include "player_actions.h"
#include "punishments.h"
#include "server_actions.h"
#include "settings.h"
#include "steam.h"
#include "common.h"

struct VoteState
{
    bool running;
    int id;
    int type;
    int adminSlot;
    uint64 adminSteamId;
    int targetSlot;
    uint64 targetSteamId;
    std::string targetName;
    std::string map;
    int yes;
    int no;
    bool voted[MAX_SLOTS];
};

static VoteState g_Vote;
static int g_NextVoteId = 1;

static const char* g_VoteTitleKeys[4] = { "Vote_Title_Kick", "Vote_Title_Ban", "Vote_Title_Mute", "Vote_Title_Map" };
static const char* g_VoteItemKeys[4] = { "Item_VoteKick", "Item_VoteBan", "Item_VoteMute", "Item_VoteMap" };
static const char* g_VoteIdentities[4] = { "votekick", "voteban", "votemute", "votemap" };

bool IsVoteRunning()
{
    return g_Vote.running;
}

static std::string VoteTitle()
{
    std::string title = Text(g_VoteTitleKeys[g_Vote.type]);
    Replace(title, "{target}", g_Vote.targetName);
    Replace(title, "{map}", MapTitle(g_Vote.map));
    int minutes = g_Vote.type == VOTE_BAN ? g_Settings.voteBanMinutes : g_Settings.voteMuteMinutes;
    Replace(title, "{time}", FormatDuration(minutes * 60));
    return title;
}

static int InitiatorSlot()
{
    if (IsSameClient(g_Vote.adminSlot, g_Vote.adminSteamId))
        return g_Vote.adminSlot;
    return CONSOLE_SLOT;
}

static void ApplyVoteResult()
{
    int admin = InitiatorSlot();
    std::string reason = Text("VoteReason");
    bool targetOnline = IsSameClient(g_Vote.targetSlot, g_Vote.targetSteamId);

    switch (g_Vote.type)
    {
    case VOTE_KICK:
        if (targetOnline)
            KickWithAnnounce(g_Vote.targetSlot, admin, reason);
        break;
    case VOTE_BAN:
        if (targetOnline)
            PunishPlayer(g_Vote.targetSlot, RT_BAN, g_Settings.voteBanMinutes * 60, reason, admin, true, true);
        else
            PunishOffline(g_Vote.targetSteamId, g_Vote.targetName, RT_BAN, g_Settings.voteBanMinutes * 60, reason, admin);
        break;
    case VOTE_MUTE:
        if (targetOnline)
            PunishPlayer(g_Vote.targetSlot, RT_MUTE, g_Settings.voteMuteMinutes * 60, reason, admin, true, true);
        else
            PunishOffline(g_Vote.targetSteamId, g_Vote.targetName, RT_MUTE, g_Settings.voteMuteMinutes * 60, reason, admin);
        break;
    case VOTE_MAP:
        ChangeMap(admin, g_Vote.map);
        break;
    }
}

static void FinishVote(int voteId)
{
    if (!g_Vote.running || g_Vote.id != voteId)
        return;
    g_Vote.running = false;

    int total = g_Vote.yes + g_Vote.no;
    bool passed = total > 0 && g_Vote.yes * 100 >= g_Settings.votePercent * total;

    std::string result = Text(passed ? "Vote_Passed" : "Vote_Failed");
    Replace(result, "{title}", VoteTitle());
    Replace(result, "{yes}", std::to_string(g_Vote.yes));
    Replace(result, "{no}", std::to_string(g_Vote.no));
    ChatToAll(result);
    META_CONPRINTF("[Verdict] Vote \"%s\": %d yes, %d no\n", StripColors(VoteTitle()).c_str(), g_Vote.yes, g_Vote.no);

    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (IsRealClient(slot) && !g_Vote.voted[slot])
            g_pMenus->ClosePlayerMenu(slot);
    }

    if (passed)
        ApplyVoteResult();
}

static void ShowBallot(int slot)
{
    Menu menu;
    g_pMenus->SetTitleMenu(menu, VoteTitle().c_str());

    std::string yes = std::to_string(g_Vote.id) + "|yes";
    std::string no = std::to_string(g_Vote.id) + "|no";
    g_pMenus->AddItemMenu(menu, yes.c_str(), Phrase("Vote_Yes"));
    g_pMenus->AddItemMenu(menu, no.c_str(), Phrase("Vote_No"));

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, false);
    g_pMenus->SetCallback(menu, [](const char* back, const char* front, int item, int slot) {
        if (item >= 7 || !back || !IsValidSlot(slot))
            return;

        std::string text = back;
        size_t separator = text.find('|');
        if (separator == std::string::npos)
            return;

        int voteId = atoi(text.substr(0, separator).c_str());
        if (!g_Vote.running || g_Vote.id != voteId || g_Vote.voted[slot])
            return;

        g_Vote.voted[slot] = true;
        if (text.substr(separator + 1) == "yes")
            g_Vote.yes++;
        else
            g_Vote.no++;
        g_pMenus->ClosePlayerMenu(slot);
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static bool BeginVote(int adminSlot, int type)
{
    if (g_Vote.running)
    {
        if (IsValidSlot(adminSlot))
            ChatTo(adminSlot, Text("VoteAlreadyRunning"));
        return false;
    }

    g_Vote.running = true;
    g_Vote.id = g_NextVoteId++;
    g_Vote.type = type;
    g_Vote.adminSlot = adminSlot;
    g_Vote.adminSteamId = ClientSteamId(adminSlot);
    g_Vote.yes = 0;
    g_Vote.no = 0;
    for (int slot = 0; slot < MAX_SLOTS; slot++)
        g_Vote.voted[slot] = false;
    return true;
}

static void LaunchBallots()
{
    std::string announce = Text("Vote_Started");
    Replace(announce, "{admin}", AdminDisplayName(InitiatorSlot()));
    Replace(announce, "{title}", VoteTitle());
    ChatToAll(announce);

    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (IsRealClient(slot))
            ShowBallot(slot);
    }

    int voteId = g_Vote.id;
    g_pUtils->CreateTimer((float)g_Settings.voteDuration, [voteId]() {
        FinishVote(voteId);
        return -1.0f;
    });
}

bool StartPlayerVote(int adminSlot, int type, int targetSlot)
{
    if (type < VOTE_KICK || type > VOTE_MUTE || !IsRealClient(targetSlot))
        return false;
    if (!BeginVote(adminSlot, type))
        return false;

    g_Vote.targetSlot = targetSlot;
    g_Vote.targetSteamId = ClientSteamId(targetSlot);
    g_Vote.targetName = ClientName(targetSlot);
    g_Vote.map.clear();
    LaunchBallots();
    return true;
}

bool StartMapVote(int adminSlot, const std::string& map)
{
    if (map.empty())
        return false;
    if (!BeginVote(adminSlot, VOTE_MAP))
        return false;

    g_Vote.targetSlot = -1;
    g_Vote.targetSteamId = 0;
    g_Vote.targetName.clear();
    g_Vote.map = map;
    LaunchBallots();
    return true;
}

static void ShowVoteTargets(int slot, int type)
{
    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase(g_VoteItemKeys[type]));

    bool found = false;
    for (int target = 0; target < MAX_SLOTS; target++)
    {
        if (!IsRealClient(target) || target == slot || !CanTarget(slot, target))
            continue;
        std::string back = std::to_string(target) + ":" + SteamIdToText(ClientSteamId(target));
        g_pMenus->AddItemMenu(menu, back.c_str(), g_pMenus->escapeString(ClientName(target)).c_str());
        found = true;
    }
    if (!found)
        g_pMenus->AddItemMenu(menu, "", Phrase("NoPlayers"), ITEM_DISABLED);

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [type](const char* back, const char* front, int item, int slot) {
        if (item == 7)
        {
            ShowCategory(slot, "votes");
            return;
        }
        if (item >= 7 || !back)
            return;

        std::string text = back;
        size_t colon = text.find(':');
        if (colon == std::string::npos)
            return;
        int target = atoi(text.substr(0, colon).c_str());
        uint64 steamId = strtoull(text.substr(colon + 1).c_str(), nullptr, 10);
        if (!IsSameClient(target, steamId))
        {
            ChatTo(slot, Text("PlayerLeft"));
            return;
        }
        StartPlayerVote(slot, type, target);
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

static void ShowVoteMaps(int slot)
{
    Menu menu;
    g_pMenus->SetTitleMenu(menu, Phrase("Item_VoteMap"));

    for (size_t i = 0; i < g_Settings.maps.size(); i++)
        g_pMenus->AddItemMenu(menu, g_Settings.maps[i].value.c_str(), g_Settings.maps[i].title.c_str());
    if (g_Settings.maps.empty())
        g_pMenus->AddItemMenu(menu, "", Phrase("NoMaps"), ITEM_DISABLED);

    g_pMenus->SetExitMenu(menu, true);
    g_pMenus->SetBackMenu(menu, true);
    g_pMenus->SetCallback(menu, [](const char* back, const char* front, int item, int slot) {
        if (item == 7)
        {
            ShowCategory(slot, "votes");
            return;
        }
        if (item < 7 && back && back[0])
            StartMapVote(slot, back);
    });
    g_pMenus->DisplayPlayerMenu(menu, slot, true, true);
}

void RegisterVoteMenuItems()
{
    RegisterMenuCategory("votes", "Category_Votes", nullptr);
    for (int type = VOTE_KICK; type <= VOTE_MAP; type++)
    {
        RegisterMenuItem(g_VoteIdentities[type], g_VoteItemKeys[type], "votes", "@admin/vote", nullptr,
            [type](int slot, const char*, const char*, const char*) {
                if (type == VOTE_MAP)
                    ShowVoteMaps(slot);
                else
                    ShowVoteTargets(slot, type);
            });
    }
}
