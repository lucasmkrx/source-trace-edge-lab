#include "sourcetrace/simulator/fault_injection.hpp"

#include "sourcetrace/protocol/frame.hpp"

#include <algorithm>

namespace sourcetrace::simulator {

bool SimulatedTransport::connected() const { return connected_; }

bool SimulatedTransport::send(std::span<const std::byte> frame) {
  transmissions_.emplace_back(frame.begin(), frame.end());
  if (!connected_) {
    return false;
  }
  if (failing_sends_ > 0) {
    --failing_sends_;
    return false;
  }
  applied_frames_.insert(protocol::toHex(frame));
  return true;
}

void SimulatedTransport::setConnected(bool connected) { connected_ = connected; }

void SimulatedTransport::failNextSends(std::size_t count) { failing_sends_ = count; }

const std::vector<std::vector<std::byte>> &SimulatedTransport::transmissions() const {
  return transmissions_;
}

std::size_t SimulatedTransport::appliedEffects() const { return applied_frames_.size(); }

FaultInjectingStore::FaultInjectingStore(runtime::IControllerStore &delegate)
    : delegate_(delegate) {}

void FaultInjectingStore::failNextCommits(std::size_t count) { failing_commits_ = count; }

void FaultInjectingStore::failNextAcknowledgements(std::size_t count) {
  failing_acknowledgements_ = count;
}

runtime::RecoveryState FaultInjectingStore::recover() { return delegate_.recover(); }

bool FaultInjectingStore::hasCapacity(std::size_t records, std::size_t bytes) {
  return delegate_.hasCapacity(records, bytes);
}

runtime::StoreStatus FaultInjectingStore::commit(const runtime::CommitBatch &batch) {
  if (failing_commits_ > 0) {
    --failing_commits_;
    return runtime::StoreStatus::io_error;
  }
  return delegate_.commit(batch);
}

std::vector<runtime::StoredFrame> FaultInjectingStore::pending(std::size_t limit) {
  return delegate_.pending(limit);
}

runtime::StoreStatus FaultInjectingStore::recordAttempt(const std::string &event_id,
                                                        const runtime::RetryState &retry) {
  return delegate_.recordAttempt(event_id, retry);
}

runtime::StoreStatus FaultInjectingStore::acknowledge(const std::string &event_id) {
  if (failing_acknowledgements_ > 0) {
    --failing_acknowledgements_;
    return runtime::StoreStatus::io_error;
  }
  return delegate_.acknowledge(event_id);
}

runtime::StoreStatus FaultInjectingStore::resetRetryBudget() {
  return delegate_.resetRetryBudget();
}

runtime::QueueStats FaultInjectingStore::queueStats() { return delegate_.queueStats(); }

std::vector<runtime::AuditRecord> FaultInjectingStore::auditTrail(std::size_t limit) {
  return delegate_.auditTrail(limit);
}

DeterministicNoise::DeterministicNoise(std::uint32_t seed) : state_(seed) {}

bool DeterministicNoise::occupancySample(bool actual_occupied, std::uint8_t noise_percent) {
  state_ = state_ * 1'664'525U + 1'013'904'223U;
  const auto sample = static_cast<std::uint8_t>((state_ >> 24U) % 100U);
  return sample < noise_percent ? !actual_occupied : actual_occupied;
}

} // namespace sourcetrace::simulator
