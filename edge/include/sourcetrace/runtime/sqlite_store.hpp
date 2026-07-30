#pragma once

#include "sourcetrace/runtime/store.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <string>

namespace sourcetrace::runtime {

class SQLiteStore final : public IControllerStore {
public:
  explicit SQLiteStore(const std::filesystem::path &path, QueueLimits limits = {});
  ~SQLiteStore() override;

  SQLiteStore(const SQLiteStore &) = delete;
  SQLiteStore &operator=(const SQLiteStore &) = delete;
  SQLiteStore(SQLiteStore &&) = delete;
  SQLiteStore &operator=(SQLiteStore &&) = delete;

  [[nodiscard]] RecoveryState recover() override;
  [[nodiscard]] bool hasCapacity(std::size_t records, std::size_t bytes) override;
  [[nodiscard]] StoreStatus commit(const CommitBatch &batch) override;
  [[nodiscard]] std::vector<StoredFrame> pending(std::size_t limit) override;
  [[nodiscard]] StoreStatus recordAttempt(const std::string &event_id,
                                          const RetryState &retry) override;
  [[nodiscard]] StoreStatus acknowledge(const std::string &event_id) override;
  [[nodiscard]] StoreStatus resetRetryBudget() override;
  [[nodiscard]] QueueStats queueStats() override;
  [[nodiscard]] std::vector<AuditRecord> auditTrail(std::size_t limit) override;

private:
  void initialize();
  void execute(const char *sql);

  sqlite3 *database_{nullptr};
  QueueLimits limits_;
};

} // namespace sourcetrace::runtime
