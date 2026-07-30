#pragma once

#include "sourcetrace/core/types.hpp"
#include "sourcetrace/protocol/frame.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sourcetrace::runtime {

struct QueueLimits {
  std::size_t max_records{256};
  std::size_t max_bytes{std::size_t{1024} * 1024};
};

struct QueueStats {
  std::size_t records{0};
  std::size_t bytes{0};
};

struct AuditRecord {
  std::uint64_t at_ms{0};
  core::AuditCode code{core::AuditCode::boot_completed};
  std::string detail;
};

struct StoredFrame {
  std::string event_id;
  std::uint64_t sequence{0};
  std::vector<std::byte> bytes;
  std::uint32_t attempts{0};
  std::uint64_t next_retry_ms{0};
};

struct ReplayRecord {
  std::uint64_t sequence{0};
  protocol::Signature signature{};
};

struct CommitBatch {
  core::ControllerState state;
  std::uint64_t next_outbound_sequence{1};
  std::vector<AuditRecord> audits;
  std::vector<StoredFrame> outbound;
  std::optional<ReplayRecord> replay;
};

struct RecoveryState {
  std::optional<core::ControllerState> state;
  std::uint64_t next_outbound_sequence{1};
  std::optional<ReplayRecord> replay;
};

struct RetryState {
  std::uint32_t attempts{0};
  std::uint64_t next_retry_ms{0};
};

enum class StoreStatus : std::uint8_t {
  ok,
  queue_full,
  io_error,
};

class IControllerStore {
public:
  virtual ~IControllerStore() = default;
  [[nodiscard]] virtual RecoveryState recover() = 0;
  [[nodiscard]] virtual bool hasCapacity(std::size_t records, std::size_t bytes) = 0;
  [[nodiscard]] virtual StoreStatus commit(const CommitBatch &batch) = 0;
  [[nodiscard]] virtual std::vector<StoredFrame> pending(std::size_t limit) = 0;
  [[nodiscard]] virtual StoreStatus recordAttempt(const std::string &event_id,
                                                  const RetryState &retry) = 0;
  [[nodiscard]] virtual StoreStatus acknowledge(const std::string &event_id) = 0;
  [[nodiscard]] virtual StoreStatus resetRetryBudget() = 0;
  [[nodiscard]] virtual QueueStats queueStats() = 0;
  [[nodiscard]] virtual std::vector<AuditRecord> auditTrail(std::size_t limit) = 0;
};

} // namespace sourcetrace::runtime
