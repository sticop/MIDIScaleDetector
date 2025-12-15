#include "Database.h"
#include <sstream>
#include <ctime>

namespace MIDIScaleDetector {

Database::Database() : db(nullptr), lastError("") {}

Database::~Database() {
    close();
}

bool Database::initialize(const std::string& dbPath) {
    if (db != nullptr) {
        close();
    }

    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        lastError = "Failed to open database: " + std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        db = nullptr;
        return false;
    }

    return createTables();
}

void Database::close() {
    if (db != nullptr) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool Database::createTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS midi_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT UNIQUE NOT NULL,
            file_name TEXT NOT NULL,
            file_size INTEGER,
            last_modified INTEGER,
            detected_key TEXT,
            detected_scale TEXT,
            confidence REAL,
            tempo REAL,
            duration REAL,
            total_notes INTEGER,
            average_pitch REAL,
            chord_progression TEXT,
            date_added INTEGER,
            date_analyzed INTEGER
        );

        CREATE INDEX IF NOT EXISTS idx_key ON midi_files(detected_key);
        CREATE INDEX IF NOT EXISTS idx_scale ON midi_files(detected_scale);
        CREATE INDEX IF NOT EXISTS idx_tempo ON midi_files(tempo);
        CREATE INDEX IF NOT EXISTS idx_confidence ON midi_files(confidence);
        CREATE INDEX IF NOT EXISTS idx_path ON midi_files(file_path);
    )";

    return executeSQL(sql);
}

bool Database::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        lastError = "SQL error: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool Database::addFile(const MIDIFileEntry& entry) {
    const char* sql = R"(
        INSERT INTO midi_files (
            file_path, file_name, file_size, last_modified,
            detected_key, detected_scale, confidence, tempo, duration,
            total_notes, average_pitch, chord_progression,
            date_added, date_analyzed
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        lastError = "Failed to prepare statement: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, entry.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entry.fileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, entry.fileSize);
    sqlite3_bind_int64(stmt, 4, entry.lastModified);
    sqlite3_bind_text(stmt, 5, entry.detectedKey.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, entry.detectedScale.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 7, entry.confidence);
    sqlite3_bind_double(stmt, 8, entry.tempo);
    sqlite3_bind_double(stmt, 9, entry.duration);
    sqlite3_bind_int(stmt, 10, entry.totalNotes);
    sqlite3_bind_double(stmt, 11, entry.averagePitch);
    sqlite3_bind_text(stmt, 12, entry.chordProgression.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 13, entry.dateAdded);
    sqlite3_bind_int64(stmt, 14, entry.dateAnalyzed);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        lastError = "Failed to insert file: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    return true;
}

bool Database::updateFile(const MIDIFileEntry& entry) {
    const char* sql = R"(
        UPDATE midi_files SET
            file_name = ?, file_size = ?, last_modified = ?,
            detected_key = ?, detected_scale = ?, confidence = ?,
            tempo = ?, duration = ?, total_notes = ?,
            average_pitch = ?, chord_progression = ?, date_analyzed = ?
        WHERE file_path = ?
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        lastError = "Failed to prepare statement: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, entry.fileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, entry.fileSize);
    sqlite3_bind_int64(stmt, 3, entry.lastModified);
    sqlite3_bind_text(stmt, 4, entry.detectedKey.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, entry.detectedScale.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, entry.confidence);
    sqlite3_bind_double(stmt, 7, entry.tempo);
    sqlite3_bind_double(stmt, 8, entry.duration);
    sqlite3_bind_int(stmt, 9, entry.totalNotes);
    sqlite3_bind_double(stmt, 10, entry.averagePitch);
    sqlite3_bind_text(stmt, 11, entry.chordProgression.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 12, entry.dateAnalyzed);
    sqlite3_bind_text(stmt, 13, entry.filePath.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        lastError = "Failed to update file: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    return true;
}

bool Database::removeFile(const std::string& filePath) {
    const char* sql = "DELETE FROM midi_files WHERE file_path = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        lastError = "Failed to prepare statement: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::fileExists(const std::string& filePath) {
    const char* sql = "SELECT COUNT(*) FROM midi_files WHERE file_path = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    int count = 0;

    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    return count > 0;
}

MIDIFileEntry Database::getFile(int id) {
    const char* sql = "SELECT * FROM midi_files WHERE id = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    MIDIFileEntry entry;

    if (rc != SQLITE_OK) {
        return entry;
    }

    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        entry = parseRow(stmt);
    }

    sqlite3_finalize(stmt);

    return entry;
}

MIDIFileEntry Database::getFile(const std::string& filePath) {
    const char* sql = "SELECT * FROM midi_files WHERE file_path = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    MIDIFileEntry entry;

    if (rc != SQLITE_OK) {
        return entry;
    }

    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        entry = parseRow(stmt);
    }

    sqlite3_finalize(stmt);

    return entry;
}

std::vector<MIDIFileEntry> Database::getAllFiles() {
    const char* sql = "SELECT * FROM midi_files ORDER BY file_name";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    std::vector<MIDIFileEntry> files;

    if (rc != SQLITE_OK) {
        return files;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        files.push_back(parseRow(stmt));
    }

    sqlite3_finalize(stmt);

    return files;
}

std::vector<MIDIFileEntry> Database::search(const SearchCriteria& criteria) {
    std::string sql = buildSearchQuery(criteria);

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    std::vector<MIDIFileEntry> files;

    if (rc != SQLITE_OK) {
        lastError = "Failed to prepare search: " + std::string(sqlite3_errmsg(db));
        return files;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        files.push_back(parseRow(stmt));
    }

    sqlite3_finalize(stmt);

    return files;
}

std::string Database::buildSearchQuery(const SearchCriteria& criteria) {
    std::ostringstream query;
    query << "SELECT * FROM midi_files WHERE 1=1";

    if (!criteria.keyFilter.empty()) {
        query << " AND detected_key = '" << criteria.keyFilter << "'";
    }

    if (!criteria.scaleFilter.empty()) {
        query << " AND detected_scale = '" << criteria.scaleFilter << "'";
    }

    query << " AND confidence >= " << criteria.minConfidence;
    query << " AND confidence <= " << criteria.maxConfidence;
    query << " AND tempo >= " << criteria.minTempo;
    query << " AND tempo <= " << criteria.maxTempo;
    query << " AND duration >= " << criteria.minDuration;
    query << " AND duration <= " << criteria.maxDuration;

    if (!criteria.pathFilter.empty()) {
        query << " AND file_path LIKE '%" << criteria.pathFilter << "%'";
    }

    query << " ORDER BY file_name";

    return query.str();
}

int Database::getTotalFileCount() {
    const char* sql = "SELECT COUNT(*) FROM midi_files";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    int count = 0;

    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    return count;
}

std::vector<std::pair<std::string, int>> Database::getKeyDistribution() {
    const char* sql = R"(
        SELECT detected_key, COUNT(*) as count
        FROM midi_files
        WHERE detected_key IS NOT NULL
        GROUP BY detected_key
        ORDER BY count DESC
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    std::vector<std::pair<std::string, int>> distribution;

    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            int count = sqlite3_column_int(stmt, 1);
            distribution.push_back({key, count});
        }
    }

    sqlite3_finalize(stmt);

    return distribution;
}

std::vector<std::pair<std::string, int>> Database::getScaleDistribution() {
    const char* sql = R"(
        SELECT detected_scale, COUNT(*) as count
        FROM midi_files
        WHERE detected_scale IS NOT NULL
        GROUP BY detected_scale
        ORDER BY count DESC
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    std::vector<std::pair<std::string, int>> distribution;

    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string scale = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            int count = sqlite3_column_int(stmt, 1);
            distribution.push_back({scale, count});
        }
    }

    sqlite3_finalize(stmt);

    return distribution;
}

bool Database::vacuum() {
    return executeSQL("VACUUM");
}

bool Database::rebuildIndex() {
    return executeSQL("REINDEX");
}

MIDIFileEntry Database::parseRow(sqlite3_stmt* stmt) {
    MIDIFileEntry entry;

    entry.id = sqlite3_column_int(stmt, 0);
    entry.filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    entry.fileName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    entry.fileSize = sqlite3_column_int64(stmt, 3);
    entry.lastModified = sqlite3_column_int64(stmt, 4);

    if (sqlite3_column_text(stmt, 5)) {
        entry.detectedKey = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    }
    if (sqlite3_column_text(stmt, 6)) {
        entry.detectedScale = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    }

    entry.confidence = sqlite3_column_double(stmt, 7);
    entry.tempo = sqlite3_column_double(stmt, 8);
    entry.duration = sqlite3_column_double(stmt, 9);
    entry.totalNotes = sqlite3_column_int(stmt, 10);
    entry.averagePitch = sqlite3_column_double(stmt, 11);

    if (sqlite3_column_text(stmt, 12)) {
        entry.chordProgression = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
    }

    entry.dateAdded = sqlite3_column_int64(stmt, 13);
    entry.dateAnalyzed = sqlite3_column_int64(stmt, 14);

    return entry;
}

} // namespace MIDIScaleDetector
