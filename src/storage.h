#pragma once

#include "database.h"

void ConnectDatabase(sql::IApi& api);
void DisconnectDatabase(bool providerLoaded = true);
bool IsDatabaseReady();
int ServerId();

std::string TablePrefix();
std::string Table(const char* name);
std::string PunishServerFilter(const char* column);
std::string AdminServerFilter(const char* adminColumn);

void OnDatabaseReady();
