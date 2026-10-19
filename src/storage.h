#pragma once

#include <string>
#include <vector>
#include "sql_mm.h"

void ConnectDatabase();
bool IsDatabaseReady();
int ServerId();

std::string TablePrefix();
std::string Table(const char* name);
std::string Escape(const std::string& text);
std::string Quote(const std::string& text);

void RunQuery(const std::string& query);
void RunQuery(const std::string& query, QueryCallbackFunc callback);
ISQLResult* ResultOf(ISQLQuery* query);
std::string ReadString(ISQLResult* result, int column);
int ReadInt(ISQLResult* result, int column, int fallback);

std::string PunishServerFilter(const char* column);
std::string AdminServerFilter(const char* adminColumn);

void OnDatabaseReady();
