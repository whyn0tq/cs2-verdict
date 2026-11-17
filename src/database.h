#pragma once

#include "sql.h"

bool OpenDatabase(sql::IApi& api, const sql::Config& config, sql::OnConnect callback);
void CloseDatabase(bool providerLoaded = true);

std::string Quote(const std::string& text);
void RunQuery(std::string query, sql::OnResult callback = {});
void RunTransaction(sql::Tx transaction, sql::OnCommit callback);

int ReadInt(const sql::Result& result, int column, int fallback);
