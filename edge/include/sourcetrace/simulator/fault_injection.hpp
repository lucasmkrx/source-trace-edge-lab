#pragma once

#include "sourcetrace/core/interfaces.hpp"
#include "sourcetrace/runtime/store.hpp"

#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace sourcetrace::simulator {

class SimulatedTransport final : public core::ITransport {
public:
  [[nodiscard]] bool connected() const override;
  [[nodiscard]] bool send(std::span<const std::byte> frame) override;

  void setConnected(bool connected);
  void failNextSends(std::size_t count);
  [[nodiscard]] const std::vector<std::vector<std::byte>> &transmissions() const;
  [[nodiscard]] std::size_t appliedEffects() const;

private:
  bool connected_{false};
  std::size_t failing_sends_{0};
  std::vector<std::vector<std::byte>> transmissions_;
  std::set<std::string> applied_frames_;
};

class FaultInjectingStore final : public runtime::IControllerStore {
public:
  explicit FaultInjectingStore(runtime::IControllerStore &delegate);

  void failNextCommits(std::size_t count);
  void failNextAcknowledgements(std::size_t count);

  [[nodiscard]] runtime::RecoveryState recover() override;
  [[nodiscard]] bool hasCapacity(std::size_t records, std::size_t bytes) override;
  [[nodiscard]] runtime::StoreStatus commit(const runtime::CommitBatch &batch) override;
  [[nodiscard]] std::vector<runtime::StoredFrame> pending(std::size_t limit) override;
  [[nodiscard]] runtime::StoreStatus recordAttempt(const std::string &event_id,
                                                   const runtime::RetryState &retry) override;
  [[nodiscard]] runtime::StoreStatus acknowledge(const std::string &event_id) override;
  [[nodiscard]] runtime::StoreStatus resetRetryBudget() override;
  [[nodiscard]] runtime::QueueStats queueStats() override;
  [[nodiscard]] std::vector<runtime::AuditRecord> auditTrail(std::size_t limit) override;

private:
  runtime::IControllerStore &delegate_;
  std::size_t failing_commits_{0};
  std::size_t failing_acknowledgements_{0};
};

class DeterministicNoise {
public:
  explicit DeterministicNoise(std::uint32_t seed);
  [[nodiscard]] bool occupancySample(bool actual_occupied, std::uint8_t noise_percent);

private:
  std::uint32_t state_;
};

} // namespace sourcetrace::simulator
