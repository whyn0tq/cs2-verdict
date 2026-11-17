#include "database.h"

static sql::IConn* g_Connection = nullptr;

bool OpenDatabase(sql::IApi& api, const sql::Config& config, sql::OnConnect callback)
{
    CloseDatabase();
    g_Connection = api.Open(config);
    if (!g_Connection)
        return false;

    g_Connection->Connect([callback = std::move(callback)](bool ok, const std::string& error) {
        if (!ok)
            CloseDatabase();
        if (callback)
            callback(ok, error);
    });
    return true;
}

void CloseDatabase(bool providerLoaded)
{
    auto* connection = std::exchange(g_Connection, nullptr);
    if (connection && providerLoaded)
        connection->Close();
}

std::string Quote(const std::string& text)
{
    return g_Connection ? "'" + g_Connection->Escape(text) + "'" : "''";
}

void RunQuery(std::string query, sql::OnResult callback)
{
    if (g_Connection)
        g_Connection->Query(std::move(query), std::move(callback));
}

void RunTransaction(sql::Tx transaction, sql::OnCommit callback)
{
    if (g_Connection)
        g_Connection->Commit(std::move(transaction), std::move(callback));
}

int ReadInt(const sql::Result& result, int column, int fallback)
{
    return result.Null(column) ? fallback : static_cast<int>(result.Int(column));
}
