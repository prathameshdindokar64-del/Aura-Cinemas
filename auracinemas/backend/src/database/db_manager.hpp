// ============================================================================
// AuraCinemas — SQLite Database Manager
// Manages DB connection lifecycle, schema init, and provides the sqlite3*
// handle for services. Thread-safe via WAL mode + busy timeout.
// ============================================================================
#pragma once

#include <sqlite3.h>

#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace aura {

class DbManager {
public:
    explicit DbManager(const std::string& db_path)
        : db_path_(db_path) {}

    ~DbManager() { close(); }

    // No copy
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    // -----------------------------------------------------------------------
    // Open the database and configure WAL + foreign keys
    // -----------------------------------------------------------------------
    bool open() {
        int rc = sqlite3_open(db_path_.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::cerr << "[DB] Failed to open: " << sqlite3_errmsg(db_) << "\n";
            return false;
        }
        // Enable WAL mode for concurrent reads
        exec("PRAGMA journal_mode = WAL;");
        exec("PRAGMA foreign_keys = ON;");
        exec("PRAGMA busy_timeout = 5000;");

        std::cout << "[DB] Opened: " << db_path_ << " (WAL mode)\n";
        return true;
    }

    void close() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
            std::cout << "[DB] Connection closed.\n";
        }
    }

    // -----------------------------------------------------------------------
    // Execute a SQL file (schema or seed data)
    // -----------------------------------------------------------------------
    bool execute_file(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[DB] Cannot open SQL file: " << filepath << "\n";
            return false;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        std::string sql = ss.str();

        char* err_msg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::cerr << "[DB] Error in " << filepath << ": " << err_msg << "\n";
            sqlite3_free(err_msg);
            return false;
        }
        std::cout << "[DB] Executed: " << filepath << "\n";
        return true;
    }

    // -----------------------------------------------------------------------
    // Execute raw SQL
    // -----------------------------------------------------------------------
    bool exec(const std::string& sql) {
        char* err_msg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::cerr << "[DB] SQL error: " << err_msg << "\n";
            sqlite3_free(err_msg);
            return false;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Query helper — calls callback per row
    // -----------------------------------------------------------------------
    using RowCallback = std::function<void(sqlite3_stmt*)>;

    bool query(const std::string& sql, RowCallback cb) {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[DB] Prepare error: " << sqlite3_errmsg(db_) << "\n";
            return false;
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cb(stmt);
        }
        sqlite3_finalize(stmt);
        return true;
    }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------
    sqlite3* handle() { return db_; }
    const std::string& path() const { return db_path_; }

    // -----------------------------------------------------------------------
    // Check if a table exists (for idempotent init)
    // -----------------------------------------------------------------------
    bool table_exists(const std::string& table_name) {
        bool exists = false;
        std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='" +
                          table_name + "'";
        query(sql, [&](sqlite3_stmt*) { exists = true; });
        return exists;
    }

private:
    std::string db_path_;
    sqlite3*    db_ = nullptr;
};

} // namespace aura
