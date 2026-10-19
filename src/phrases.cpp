#include "phrases.h"
#include "settings.h"
#include "common.h"
#include "filesystem.h"

static std::map<std::string, std::string> g_Phrases;

static void ReadPhraseFile(const char* path, bool required)
{
    KeyValues* phrases = new KeyValues("Phrases");
    if (!phrases->LoadFromFile(g_pFullFileSystem, path))
    {
        if (required)
            g_pUtils->ErrorLog("[Verdict] Failed to load %s", path);
        delete phrases;
        return;
    }

    const char* language = g_pUtils->GetLanguage();
    for (KeyValues* key = phrases->GetFirstTrueSubKey(); key; key = key->GetNextTrueSubKey())
    {
        const char* fallback = key->GetString("en", "");
        g_Phrases[key->GetName()] = key->GetString(language, fallback);
    }
    delete phrases;
}

void LoadPhrases()
{
    g_Phrases.clear();
    ReadPhraseFile("addons/translations/admin_system.phrases.txt", false);
    ReadPhraseFile("addons/translations/verdict.phrases.txt", true);
}

const char* Phrase(const char* key)
{
    std::map<std::string, std::string>::iterator found = g_Phrases.find(key);
    if (found == g_Phrases.end())
        return key;
    return found->second.c_str();
}

std::string Text(const char* key)
{
    return Phrase(key);
}

void Replace(std::string& text, const char* tag, const std::string& value)
{
    std::string search = tag;
    size_t position = text.find(search);
    while (position != std::string::npos)
    {
        text.replace(position, search.length(), value);
        position = text.find(search, position + value.length());
    }
}

std::string StripColors(const std::string& text)
{
    std::string result;
    size_t i = 0;
    while (i < text.length())
    {
        if (text[i] == '{')
        {
            size_t end = text.find('}', i);
            bool isTag = end != std::string::npos && end - i <= 16;
            for (size_t j = i + 1; isTag && j < end; j++)
            {
                if (!isupper((unsigned char)text[j]) && text[j] != '_')
                    isTag = false;
            }
            if (isTag)
            {
                i = end + 1;
                continue;
            }
        }
        result += text[i];
        i++;
    }
    return result;
}

void ChatTo(int slot, const std::string& text)
{
    if (!IsValidSlot(slot))
        return;
    std::string message = g_Settings.chatPrefix + " " + text;
    g_pUtils->PrintToChat(slot, "%s", message.c_str());
}

void ChatToAll(const std::string& text)
{
    std::string message = g_Settings.chatPrefix + " " + text;
    g_pUtils->PrintToChatAll("%s", message.c_str());
}

void ConsoleTo(int slot, const std::string& text)
{
    std::string message = "[Verdict] " + StripColors(text) + "\n";
    if (slot == CONSOLE_SLOT)
        META_CONPRINT(message.c_str());
    else if (IsValidSlot(slot))
        g_pUtils->PrintToConsole(slot, "%s", message.c_str());
}

void ReplyTo(int slot, bool fromChat, const std::string& text)
{
    if (fromChat && IsValidSlot(slot))
        ChatTo(slot, text);
    else
        ConsoleTo(slot, text);
}
