#include "sourcetrace/runtime/sqlite_store.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace sourcetrace::runtime {
namespace {

using Json = nlohmann::json;

class Statement {
public:
  Statement(sqlite3 *database, const char *sql) {
    if (sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database));
    }
  }
  ~Statement() { sqlite3_finalize(statement_); }
  Statement(const Statement &) = delete;
  Statement &operator=(const Statement &) = delete;
  [[nodiscard]] sqlite3_stmt *get() const { return statement_; }

private:
  sqlite3_stmt *statement_{nullptr};
};

sqlite3_int64 toSqlInteger(std::uint64_t value) {
  if (value > static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max())) {
    throw std::overflow_error("sequence exceeds SQLite signed integer range");
  }
  return static_cast<sqlite3_int64>(value);
}

Json stateToJson(const core::ControllerState &state) {
  return Json{
      {"mode", static_cast<std::uint8_t>(state.mode)},
      {"health", static_cast<std::uint8_t>(state.health)},
      {"indicator", static_cast<std::uint8_t>(state.indicator)},
      {"occupied", state.occupied},
      {"connected", state.connected},
      {"configurationValid", state.configuration_valid},
      {"configuration",
       {{"version", state.configuration.version},
        {"telemetryIntervalMs", state.configuration.telemetry_interval_ms},
        {"occupancyRequiredToStart", state.configuration.occupancy_required_to_start}}},
      {"monotonicMs", state.monotonic_ms},
      {"lastTelemetryMs", state.last_telemetry_ms},
      {"transitionCount", state.transition_count},
  };
}

core::ControllerState stateFromJson(const std::string &serialized) {
  const auto value = Json::parse(serialized);
  core::ControllerState state;
  state.mode = static_cast<core::ControllerMode>(value.at("mode").get<std::uint8_t>());
  state.health = static_cast<core::Health>(value.at("health").get<std::uint8_t>());
  state.indicator = static_cast<core::Indicator>(value.at("indicator").get<std::uint8_t>());
  state.occupied = value.at("occupied").get<bool>();
  state.connected = value.at("connected").get<bool>();
  state.configuration_valid = value.at("configurationValid").get<bool>();
  const auto &configuration = value.at("configuration");
  state.configuration.version = configuration.at("version").get<std::uint64_t>();
  state.configuration.telemetry_interval_ms =
      configuration.at("telemetryIntervalMs").get<std::uint64_t>();
  state.configuration.occupancy_required_to_start =
      configuration.at("occupancyRequiredToStart").get<bool>();
  state.monotonic_ms = value.at("monotonicMs").get<std::uint64_t>();
  state.last_telemetry_ms = value.at("lastTelemetryMs").get<std::uint64_t>();
  state.transition_count = value.at("transitionCount").get<std::uint64_t>();
  return state;
}

bool stepDone(sqlite3_stmt *statement) { return sqlite3_step(statement) == SQLITE_DONE; }

} // namespace

SQLiteStore::SQLiteStore(const std::filesystem::path &path, QueueLimits limits)
    : limits_(limits) {
  const auto path_string = path.string();
  if (sqlite3_open_v2(path_string.c_str(), &database_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                      nullptr) != SQLITE_OK) {
    const std::string message = database_ == nullptr ? "SQLite open failed"
                                                      : sqlite3_errmsg(database_);
    if (database_ != nullptr) {
      sqlite3_close(database_);
      database_ = nullptr;
    }
    throw std::runtime_error(message);
  }
  initialize();
}

SQLiteStore::~SQLiteStore() {
  if (database_ != nullptr) {
    sqlite3_close(database_);
  }
}

void SQLiteStore::execute(const char *sql) {
  char *error = nullptr;
  if (sqlite3_exec(database_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error == nullptr ? sqlite3_errmsg(database_) : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

void SQLiteStore::initialize() {
  execute("PRAGMA journal_mode=DELETE;");
  execute("PRAGMA synchronous=FULL;");
  execute("PRAGMA foreign_keys=ON;");
  execute("PRAGMA busy_timeout=2000;");
  execute(
      "CREATE TABLE IF NOT EXISTS controller_state("
      "id INTEGER PRIMARY KEY CHECK(id=1), payload TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS metadata("
      "key TEXT PRIMARY KEY, value INTEGER NOT NULL);"
      "CREATE TABLE IF NOT EXISTS replay_state("
      "id INTEGER PRIMARY KEY CHECK(id=1), sequence INTEGER NOT NULL,"
      "signature BLOB NOT NULL CHECK(length(signature)=64));"
      "CREATE TABLE IF NOT EXISTS outbound_queue("
      "event_id TEXT PRIMARY KEY, sequence INTEGER NOT NULL UNIQUE,"
      "frame BLOB NOT NULL, attempts INTEGER NOT NULL DEFAULT 0,"
      "next_retry_ms INTEGER NOT NULL DEFAULT 0);"
      "CREATE TABLE IF NOT EXISTS audit("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, at_ms INTEGER NOT NULL,"
      "code INTEGER NOT NULL, detail TEXT NOT NULL);"
      "CREATE TRIGGER IF NOT EXISTS audit_no_update BEFORE UPDATE ON audit "
      "BEGIN SELECT RAISE(ABORT,'audit records are append-only'); END;"
      "CREATE TRIGGER IF NOT EXISTS audit_no_delete BEFORE DELETE ON audit "
      "BEGIN SELECT RAISE(ABORT,'audit records are append-only'); END;"
      "INSERT OR IGNORE INTO metadata(key,value) VALUES('next_outbound_sequence',1);");
}

RecoveryState SQLiteStore::recover() {
  RecoveryState recovery;
  {
    Statement statement(database_, "SELECT payload FROM controller_state WHERE id=1;");
    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
      const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(statement.get(), 0));
      if (text == nullptr) {
        throw std::runtime_error("controller checkpoint is null");
      }
      recovery.state = stateFromJson(text);
    }
  }
  {
    Statement statement(
        database_, "SELECT value FROM metadata WHERE key='next_outbound_sequence';");
    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
      recovery.next_outbound_sequence =
          static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 0));
    }
  }
  {
    Statement statement(database_, "SELECT sequence,signature FROM replay_state WHERE id=1;");
    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
      ReplayRecord replay;
      replay.sequence = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 0));
      const auto *data =
          static_cast<const std::byte *>(sqlite3_column_blob(statement.get(), 1));
      const auto size = sqlite3_column_bytes(statement.get(), 1);
      if (data == nullptr || size != static_cast<int>(replay.signature.size())) {
        throw std::runtime_error("stored replay signature has invalid size");
      }
      std::copy_n(data, replay.signature.size(), replay.signature.begin());
      recovery.replay = replay;
    }
  }
  return recovery;
}

QueueStats SQLiteStore::queueStats() {
  Statement statement(
      database_, "SELECT count(*),coalesce(sum(length(frame)),0) FROM outbound_queue;");
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    throw std::runtime_error(sqlite3_errmsg(database_));
  }
  return QueueStats{
      static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 0)),
      static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 1)),
  };
}

bool SQLiteStore::hasCapacity(std::size_t records, std::size_t bytes) {
  const auto current = queueStats();
  return records <= limits_.max_records - std::min(current.records, limits_.max_records) &&
         bytes <= limits_.max_bytes - std::min(current.bytes, limits_.max_bytes);
}

StoreStatus SQLiteStore::commit(const CommitBatch &batch) {
  std::size_t added_bytes = 0;
  for (const auto &frame : batch.outbound) {
    if (frame.bytes.size() > limits_.max_bytes - std::min(added_bytes, limits_.max_bytes)) {
      return StoreStatus::queue_full;
    }
    added_bytes += frame.bytes.size();
  }
  if (!hasCapacity(batch.outbound.size(), added_bytes)) {
    return StoreStatus::queue_full;
  }

  try {
    execute("BEGIN IMMEDIATE;");
    const auto state_json = stateToJson(batch.state).dump();
    {
      Statement statement(
          database_,
          "INSERT INTO controller_state(id,payload) VALUES(1,?1)"
          " ON CONFLICT(id) DO UPDATE SET payload=excluded.payload;");
      sqlite3_bind_text(statement.get(), 1, state_json.c_str(),
                        static_cast<int>(state_json.size()), SQLITE_TRANSIENT);
      if (!stepDone(statement.get())) {
        throw std::runtime_error(sqlite3_errmsg(database_));
      }
    }
    {
      Statement statement(
          database_,
          "UPDATE metadata SET value=?1 WHERE key='next_outbound_sequence';");
      sqlite3_bind_int64(statement.get(), 1, toSqlInteger(batch.next_outbound_sequence));
      if (!stepDone(statement.get())) {
        throw std::runtime_error(sqlite3_errmsg(database_));
      }
    }
    if (batch.replay) {
      Statement statement(
          database_,
          "INSERT INTO replay_state(id,sequence,signature) VALUES(1,?1,?2)"
          " ON CONFLICT(id) DO UPDATE SET sequence=excluded.sequence,"
          "signature=excluded.signature;");
      sqlite3_bind_int64(statement.get(), 1, toSqlInteger(batch.replay->sequence));
      sqlite3_bind_blob(statement.get(), 2, batch.replay->signature.data(),
                        static_cast<int>(batch.replay->signature.size()), SQLITE_TRANSIENT);
      if (!stepDone(statement.get())) {
        throw std::runtime_error(sqlite3_errmsg(database_));
      }
    }
    for (const auto &audit : batch.audits) {
      Statement statement(
          database_, "INSERT INTO audit(at_ms,code,detail) VALUES(?1,?2,?3);");
      sqlite3_bind_int64(statement.get(), 1, toSqlInteger(audit.at_ms));
      sqlite3_bind_int(statement.get(), 2, static_cast<int>(audit.code));
      sqlite3_bind_text(statement.get(), 3, audit.detail.c_str(),
                        static_cast<int>(audit.detail.size()), SQLITE_TRANSIENT);
      if (!stepDone(statement.get())) {
        throw std::runtime_error(sqlite3_errmsg(database_));
      }
    }
    for (const auto &frame : batch.outbound) {
      Statement statement(
          database_,
          "INSERT INTO outbound_queue(event_id,sequence,frame,attempts,next_retry_ms)"
          " VALUES(?1,?2,?3,?4,?5);");
      sqlite3_bind_text(statement.get(), 1, frame.event_id.c_str(),
                        static_cast<int>(frame.event_id.size()), SQLITE_TRANSIENT);
      sqlite3_bind_int64(statement.get(), 2, toSqlInteger(frame.sequence));
      sqlite3_bind_blob(statement.get(), 3, frame.bytes.data(),
                        static_cast<int>(frame.bytes.size()), SQLITE_TRANSIENT);
      sqlite3_bind_int(statement.get(), 4, static_cast<int>(frame.attempts));
      sqlite3_bind_int64(statement.get(), 5, toSqlInteger(frame.next_retry_ms));
      if (!stepDone(statement.get())) {
        throw std::runtime_error(sqlite3_errmsg(database_));
      }
    }
    execute("COMMIT;");
    return StoreStatus::ok;
  } catch (const std::exception &) {
    static_cast<void>(sqlite3_exec(database_, "ROLLBACK;", nullptr, nullptr, nullptr));
    return StoreStatus::io_error;
  }
}

std::vector<StoredFrame> SQLiteStore::pending(std::size_t limit) {
  Statement statement(
      database_,
      "SELECT event_id,sequence,frame,attempts,next_retry_ms FROM outbound_queue"
      " ORDER BY sequence LIMIT ?1;");
  sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(limit));
  std::vector<StoredFrame> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    StoredFrame frame;
    const auto *event_id =
        reinterpret_cast<const char *>(sqlite3_column_text(statement.get(), 0));
    frame.event_id = event_id == nullptr ? "" : event_id;
    frame.sequence = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 1));
    const auto *data = static_cast<const std::byte *>(sqlite3_column_blob(statement.get(), 2));
    const auto size = sqlite3_column_bytes(statement.get(), 2);
    if (data == nullptr || size < 0) {
      throw std::runtime_error("queued frame blob is invalid");
    }
    frame.bytes.assign(data, data + size);
    frame.attempts = static_cast<std::uint32_t>(sqlite3_column_int(statement.get(), 3));
    frame.next_retry_ms =
        static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 4));
    result.push_back(std::move(frame));
  }
  return result;
}

StoreStatus SQLiteStore::recordAttempt(const std::string &event_id,
                                       const RetryState &retry) {
  try {
    Statement statement(
        database_,
        "UPDATE outbound_queue SET attempts=?1,next_retry_ms=?2 WHERE event_id=?3;");
    sqlite3_bind_int(statement.get(), 1, static_cast<int>(retry.attempts));
    sqlite3_bind_int64(statement.get(), 2, toSqlInteger(retry.next_retry_ms));
    sqlite3_bind_text(statement.get(), 3, event_id.c_str(),
                      static_cast<int>(event_id.size()), SQLITE_TRANSIENT);
    return stepDone(statement.get()) ? StoreStatus::ok : StoreStatus::io_error;
  } catch (const std::exception &) {
    return StoreStatus::io_error;
  }
}

StoreStatus SQLiteStore::acknowledge(const std::string &event_id) {
  try {
    Statement statement(database_, "DELETE FROM outbound_queue WHERE event_id=?1;");
    sqlite3_bind_text(statement.get(), 1, event_id.c_str(),
                      static_cast<int>(event_id.size()), SQLITE_TRANSIENT);
    return stepDone(statement.get()) ? StoreStatus::ok : StoreStatus::io_error;
  } catch (const std::exception &) {
    return StoreStatus::io_error;
  }
}

StoreStatus SQLiteStore::resetRetryBudget() {
  try {
    execute("UPDATE outbound_queue SET attempts=0,next_retry_ms=0;");
    return StoreStatus::ok;
  } catch (const std::exception &) {
    return StoreStatus::io_error;
  }
}

std::vector<AuditRecord> SQLiteStore::auditTrail(std::size_t limit) {
  Statement statement(
      database_, "SELECT at_ms,code,detail FROM audit ORDER BY id DESC LIMIT ?1;");
  sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(limit));
  std::vector<AuditRecord> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    const auto *detail =
        reinterpret_cast<const char *>(sqlite3_column_text(statement.get(), 2));
    result.push_back(AuditRecord{
        static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 0)),
        static_cast<core::AuditCode>(sqlite3_column_int(statement.get(), 1)),
        detail == nullptr ? "" : detail,
    });
  }
  std::reverse(result.begin(), result.end());
  return result;
}

} // namespace sourcetrace::runtime
