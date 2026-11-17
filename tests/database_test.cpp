#include "database.h"

#include <cassert>
#include <iostream>

namespace sql
{
class Fill
{
public:
    static Result Row()
    {
        Result result;
        result.m_names = {"id", "name", "expiry"};
        result.m_rows = {{42, "Player", nullptr}};
        result.m_id = 42;
        result.m_affected = 1;
        return result;
    }

    static Result Error()
    {
        Result result;
        result.m_error = "Query failed";
        return result;
    }
};
}

class Connection : public sql::IConn
{
public:
    void Connect(sql::OnConnect callback) override
    {
        connect = std::move(callback);
    }

    void Query(std::string statement, sql::Args args, sql::OnResult callback) override
    {
        assert(args.empty());
        query = std::move(statement);
        reply = std::move(callback);
    }

    void Commit(sql::Tx transaction, sql::OnCommit callback) override
    {
        tx = std::move(transaction);
        commit = std::move(callback);
    }

    std::string Escape(std::string_view text) const override
    {
        std::string escaped;
        for (char ch : text)
        {
            if (ch == '\'' || ch == '\\')
                escaped += '\\';
            escaped += ch;
        }
        return escaped;
    }

    sql::Kind Type() const override
    {
        return sql::Kind::MySQL;
    }

    void Close() override
    {
        ++closes;
        connect = {};
        reply = {};
        commit = {};
    }

    int closes = 0;
    std::string query;
    sql::Tx tx;
    sql::OnConnect connect;
    sql::OnResult reply;
    sql::OnCommit commit;
};

class Api : public sql::IApi
{
public:
    sql::IConn* Open(const sql::Config& config) override
    {
        assert(config.kind == sql::Kind::MySQL);
        assert(config.name == "verdict");
        assert(config.port == 3306);
        return available ? &connection : nullptr;
    }

    const char* Version() const override
    {
        return "1.3.0";
    }

    bool available = true;
    Connection connection;
};

static sql::Config Config()
{
    sql::Config config;
    config.kind = sql::Kind::MySQL;
    config.name = "verdict";
    config.port = 3306;
    return config;
}

static void TestConnection()
{
    Api api;
    api.available = false;
    assert(!OpenDatabase(api, Config(), {}));

    api.available = true;
    bool notified = false;
    assert(OpenDatabase(api, Config(), [&](bool ok, const std::string& error) {
        assert(!ok && error == "Access denied");
        assert(api.connection.closes == 1);
        notified = true;
    }));
    assert(!notified);
    std::exchange(api.connection.connect, {})(false, "Access denied");
    assert(notified);
    RunQuery("SELECT 1");
    assert(api.connection.query.empty());

    notified = false;
    assert(OpenDatabase(api, Config(), [&](bool ok, const std::string& error) {
        assert(ok && error.empty());
        notified = true;
    }));
    std::exchange(api.connection.connect, {})(true, "");
    assert(notified);
    RunQuery("SELECT 1", [](sql::Result&) { assert(false); });
    CloseDatabase();
    assert(api.connection.closes == 2);
    assert(!api.connection.reply);
    CloseDatabase();
    assert(api.connection.closes == 2);
}

static void TestQueries()
{
    Api api;
    assert(OpenDatabase(api, Config(), {}));
    std::exchange(api.connection.connect, {})(true, "");
    assert(Quote("O'Neil\\game") == "'O\\'Neil\\\\game'");

    bool read = false;
    RunQuery("SELECT id, name, expiry FROM players", [&](sql::Result& result) {
        assert(result.Ok() && result.Next());
        assert(result.Id() == 42 && result.Affected() == 1);
        assert(result.Str(1) == "Player");
        assert(ReadInt(result, 0, -1) == 42);
        assert(ReadInt(result, 2, -1) == -1);
        assert(!result.Next());
        read = true;
    });
    assert(api.connection.query == "SELECT id, name, expiry FROM players");
    auto row = sql::Fill::Row();
    api.connection.reply(row);
    assert(read);

    RunQuery("SELECT invalid", [](sql::Result& result) {
        assert(!result.Ok() && result.Error() == "Query failed");
        assert(!result.Next() && result.Affected() == 0);
    });
    auto error = sql::Fill::Error();
    api.connection.reply(error);
    CloseDatabase();
}

static void TestTransaction()
{
    Api api;
    assert(OpenDatabase(api, Config(), {}));
    std::exchange(api.connection.connect, {})(true, "");

    sql::Tx transaction;
    transaction.Add("DELETE FROM players");
    transaction.Add("INSERT INTO players VALUES (?)", {42});
    int calls = 0;
    RunTransaction(std::move(transaction), [&](bool ok, std::vector<sql::Result>& results) {
        assert(ok == (calls == 0));
        assert(results.size() == 2 && results.front().Affected() == 1);
        assert(results.back().Ok() == ok);
        ++calls;
    });
    assert(api.connection.tx.Size() == 2);
    assert(api.connection.tx.Items()[0].sql == "DELETE FROM players");
    assert(api.connection.tx.Items()[1].args[0].Int() == 42);
    std::vector<sql::Result> results = {sql::Fill::Row(), sql::Fill::Row()};
    api.connection.commit(true, results);
    results.back() = sql::Fill::Error();
    api.connection.commit(false, results);
    assert(calls == 2);

    api.connection.Close();
    CloseDatabase(false);
    RunQuery("SELECT 1");
    assert(api.connection.closes == 1 && api.connection.query.empty());
}

int main()
{
    TestConnection();
    TestQueries();
    TestTransaction();
    std::cout << "3 database tests passed\n";
}
