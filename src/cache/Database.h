#pragma once
#include <string>
#include <functional>
#include <optional>
#include <vector>
#include <memory>
#include <sqlite3.h>

namespace conduit::cache {

// sqlite wrapper that handles schema creation and migrations
// each org gets its own database file so they can't contaminate each other
class Database {
public:
    Database();
    ~Database();

    bool open(const std::string& path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    // execute SQL that doesn't return rows
    bool exec(const std::string& sql);

    // execute SQL with a row callback
    using RowCallback = std::function<void(int ncols, char** values, char** names)>;
    bool query(const std::string& sql, RowCallback callback);

    // prepared statement helpers
    struct Statement {
        sqlite3_stmt* stmt = nullptr;
        ~Statement() { if (stmt) sqlite3_finalize(stmt); }
    };

    std::unique_ptr<Statement> prepare(const std::string& sql);

    // schema management
    int schemaVersion();
    bool createSchema();
    bool migrate();

    sqlite3* raw() { return db_; }

private:
    sqlite3* db_ = nullptr;
    std::string path_;
};

} // namespace conduit::cache
