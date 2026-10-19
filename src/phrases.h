#pragma once

#include <string>

void LoadPhrases();
const char* Phrase(const char* key);
std::string Text(const char* key);
void Replace(std::string& text, const char* tag, const std::string& value);

void ChatTo(int slot, const std::string& text);
void ChatToAll(const std::string& text);
void ConsoleTo(int slot, const std::string& text);
void ReplyTo(int slot, bool fromChat, const std::string& text);
std::string StripColors(const std::string& text);
