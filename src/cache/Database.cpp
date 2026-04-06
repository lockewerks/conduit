#include "cache/Database.h"
#include "util/Logger.h"

namespace conduit::cache {

Database::Database() {}

Database::~Database() {
    close();
}

bool Database::open(const std::string& path) {
    path_ = path;
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        LOG_ERROR("failed to open database at " + path + ": " + sqlite3_errmsg(db_));
        db_ = nullptr;
        return false;
    }

    // WAL mode for better concurrent read/write performance
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA synchronous=NORMAL");
    exec("PRAGMA foreign_keys=ON");

    // create or migrate schema
    if (schemaVersion() == 0) {
        if (!createSchema()) {
            LOG_ERROR("schema creation failed");
            close();
            return false;
        }
    } else {
        migrate();
    }

    LOG_INFO("database opened: " + path);
    return true;
}

void Database::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::exec(const std::string& sql) {
    if (!db_) return false;
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string error = err ? err : "unknown";
        sqlite3_free(err);
        LOG_ERROR("SQL exec failed: " + error + " | " + sql.substr(0, 100));
        return false;
    }
    return true;
}

bool Database::query(const std::string& sql, RowCallback callback) {
    if (!db_) return false;
    char* err = nullptr;

    auto wrapper = [](void* data, int ncols, char** values, char** names) -> int {
        auto* cb = static_cast<RowCallback*>(data);
        (*cb)(ncols, values, names);
        return 0;
    };

    int rc = sqlite3_exec(db_, sql.c_str(), wrapper, &callback, &err);
    if (rc != SQLITE_OK) {
        std::string error = err ? err : "unknown";
        sqlite3_free(err);
        LOG_ERROR("SQL query failed: " + error);
        return false;
    }
    return true;
}

std::unique_ptr<Database::Statement> Database::prepare(const std::string& sql) {
    if (!db_) return nullptr;
    auto stmt = std::make_unique<Statement>();
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt->stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR(std::string("prepare failed: ") + sqlite3_errmsg(db_));
        return nullptr;
    }
    return stmt;
}

int Database::schemaVersion() {
    if (!db_) return 0;
    int version = 0;

    // check if schema_version table even exists
    bool exists = false;
    query("SELECT name FROM sqlite_master WHERE type='table' AND name='schema_version'",
          [&](int, char** vals, char**) { exists = true; });

    if (!exists) return 0;

    query("SELECT version FROM schema_version LIMIT 1",
          [&](int, char** vals, char**) {
              if (vals[0]) version = std::atoi(vals[0]);
          });
    return version;
}

bool Database::createSchema() {
    // the whole schema in one go. this is version 1.
    const char* schema = R"SQL(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER NOT NULL
        );
        INSERT INTO schema_version VALUES (1);

        CREATE TABLE IF NOT EXISTS users (
            id TEXT PRIMARY KEY,
            display_name TEXT NOT NULL DEFAULT '',
            real_name TEXT DEFAULT '',
            avatar_url_72 TEXT DEFAULT '',
            avatar_url_192 TEXT DEFAULT '',
            status_emoji TEXT DEFAULT '',
            status_text TEXT DEFAULT '',
            is_bot INTEGER DEFAULT 0,
            updated_at INTEGER DEFAULT (strftime('%s', 'now'))
        );

        CREATE TABLE IF NOT EXISTS channels (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL DEFAULT '',
            topic TEXT DEFAULT '',
            purpose TEXT DEFAULT '',
            type TEXT NOT NULL DEFAULT 'public',
            is_member INTEGER DEFAULT 0,
            is_muted INTEGER DEFAULT 0,
            is_archived INTEGER DEFAULT 0,
            member_count INTEGER DEFAULT 0,
            dm_user_id TEXT DEFAULT '',
            last_read TEXT DEFAULT '',
            updated_at INTEGER DEFAULT (strftime('%s', 'now'))
        );

        CREATE TABLE IF NOT EXISTS messages (
            channel_id TEXT NOT NULL,
            ts TEXT NOT NULL,
            thread_ts TEXT DEFAULT '',
            user_id TEXT NOT NULL DEFAULT '',
            text TEXT NOT NULL DEFAULT '',
            subtype TEXT DEFAULT '',
            is_edited INTEGER DEFAULT 0,
            edited_ts TEXT DEFAULT '',
            reply_count INTEGER DEFAULT 0,
            is_pinned INTEGER DEFAULT 0,
            reactions_json TEXT DEFAULT '[]',
            attachments_json TEXT DEFAULT '[]',
            files_json TEXT DEFAULT '[]',
            reply_users_json TEXT DEFAULT '[]',
            PRIMARY KEY (channel_id, ts)
        );

        CREATE INDEX IF NOT EXISTS idx_messages_channel_ts ON messages(channel_id, ts DESC);
        CREATE INDEX IF NOT EXISTS idx_messages_thread ON messages(channel_id, thread_ts) WHERE thread_ts != '';
        CREATE INDEX IF NOT EXISTS idx_messages_user ON messages(user_id);

        CREATE TABLE IF NOT EXISTS pins (
            channel_id TEXT NOT NULL,
            ts TEXT NOT NULL,
            pinned_by TEXT NOT NULL DEFAULT '',
            pinned_at INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (channel_id, ts)
        );

        CREATE TABLE IF NOT EXISTS custom_emoji (
            name TEXT PRIMARY KEY,
            url TEXT NOT NULL,
            is_alias INTEGER DEFAULT 0,
            alias_for TEXT DEFAULT ''
        );

        CREATE TABLE IF NOT EXISTS read_state (
            channel_id TEXT PRIMARY KEY,
            last_read_ts TEXT NOT NULL DEFAULT '',
            unread_count INTEGER DEFAULT 0,
            mention_count INTEGER DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS file_cache (
            url TEXT PRIMARY KEY,
            local_path TEXT NOT NULL,
            size_bytes INTEGER NOT NULL DEFAULT 0,
            downloaded_at INTEGER DEFAULT (strftime('%s', 'now')),
            last_accessed INTEGER DEFAULT (strftime('%s', 'now'))
        );

        CREATE TABLE IF NOT EXISTS input_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            channel_id TEXT NOT NULL,
            text TEXT NOT NULL,
            entered_at INTEGER DEFAULT (strftime('%s', 'now'))
        );

        CREATE INDEX IF NOT EXISTS idx_input_history_channel
            ON input_history(channel_id, entered_at DESC);
    )SQL";

    if (!exec(schema)) {
        LOG_ERROR("schema creation failed, this is probably bad");
        return false;
    }

    LOG_INFO("database schema created (version 1)");
    return true;
}

bool Database::migrate() {
    int version = schemaVersion();
    if (version < 1) return createSchema();

    if (version < 2) {
        LOG_INFO("migrating database schema v1 -> v2");

        // messages: add blocks and bot_id
        exec("ALTER TABLE messages ADD COLUMN blocks_json TEXT DEFAULT '[]'");
        exec("ALTER TABLE messages ADD COLUMN bot_id TEXT DEFAULT ''");

        // users: extended profile fields
        exec("ALTER TABLE users ADD COLUMN title TEXT DEFAULT ''");
        exec("ALTER TABLE users ADD COLUMN email TEXT DEFAULT ''");
        exec("ALTER TABLE users ADD COLUMN phone TEXT DEFAULT ''");
        exec("ALTER TABLE users ADD COLUMN tz TEXT DEFAULT ''");
        exec("ALTER TABLE users ADD COLUMN tz_label TEXT DEFAULT ''");
        exec("ALTER TABLE users ADD COLUMN tz_offset INTEGER DEFAULT 0");
        exec("ALTER TABLE users ADD COLUMN pronouns TEXT DEFAULT ''");
        exec("ALTER TABLE users ADD COLUMN is_admin INTEGER DEFAULT 0");
        exec("ALTER TABLE users ADD COLUMN is_owner INTEGER DEFAULT 0");

        // bookmarks
        exec(R"SQL(
            CREATE TABLE IF NOT EXISTS bookmarks (
                id TEXT PRIMARY KEY,
                channel_id TEXT NOT NULL,
                title TEXT NOT NULL DEFAULT '',
                link TEXT DEFAULT '',
                emoji TEXT DEFAULT '',
                type TEXT DEFAULT 'link',
                created_by TEXT DEFAULT '',
                date_created INTEGER DEFAULT 0
            )
        )SQL");
        exec("CREATE INDEX IF NOT EXISTS idx_bookmarks_channel ON bookmarks(channel_id)");

        // reminders
        exec(R"SQL(
            CREATE TABLE IF NOT EXISTS reminders (
                id TEXT PRIMARY KEY,
                creator TEXT NOT NULL DEFAULT '',
                text TEXT NOT NULL DEFAULT '',
                user_id TEXT NOT NULL DEFAULT '',
                time INTEGER NOT NULL DEFAULT 0,
                complete_ts INTEGER DEFAULT 0,
                is_complete INTEGER DEFAULT 0
            )
        )SQL");
        exec("CREATE INDEX IF NOT EXISTS idx_reminders_time ON reminders(time)");

        // scheduled messages
        exec(R"SQL(
            CREATE TABLE IF NOT EXISTS scheduled_messages (
                id TEXT PRIMARY KEY,
                channel_id TEXT NOT NULL,
                text TEXT NOT NULL DEFAULT '',
                post_at INTEGER NOT NULL DEFAULT 0,
                date_created INTEGER DEFAULT 0
            )
        )SQL");

        // user groups
        exec(R"SQL(
            CREATE TABLE IF NOT EXISTS user_groups (
                id TEXT PRIMARY KEY,
                handle TEXT NOT NULL DEFAULT '',
                name TEXT NOT NULL DEFAULT '',
                description TEXT DEFAULT '',
                members_json TEXT DEFAULT '[]',
                member_count INTEGER DEFAULT 0
            )
        )SQL");

        // saved items
        exec(R"SQL(
            CREATE TABLE IF NOT EXISTS saved_items (
                channel_id TEXT NOT NULL,
                message_ts TEXT NOT NULL,
                date_saved INTEGER DEFAULT (strftime('%s', 'now')),
                PRIMARY KEY (channel_id, message_ts)
            )
        )SQL");

        // drafts
        exec(R"SQL(
            CREATE TABLE IF NOT EXISTS drafts (
                channel_id TEXT PRIMARY KEY,
                text TEXT NOT NULL DEFAULT '',
                thread_ts TEXT DEFAULT '',
                updated_at INTEGER DEFAULT (strftime('%s', 'now'))
            )
        )SQL");

        // channel sections (local sidebar organization)
        exec(R"SQL(
            CREATE TABLE IF NOT EXISTS channel_sections (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                channel_ids_json TEXT DEFAULT '[]',
                sort_order INTEGER DEFAULT 0,
                collapsed INTEGER DEFAULT 0
            )
        )SQL");

        // per-channel notification overrides
        exec(R"SQL(
            CREATE TABLE IF NOT EXISTS channel_notify_prefs (
                channel_id TEXT PRIMARY KEY,
                notify_level TEXT DEFAULT 'default',
                muted INTEGER DEFAULT 0
            )
        )SQL");

        exec("UPDATE schema_version SET version = 2");
        LOG_INFO("database migration to v2 complete");
    }

    return true;
}

} // namespace conduit::cache
