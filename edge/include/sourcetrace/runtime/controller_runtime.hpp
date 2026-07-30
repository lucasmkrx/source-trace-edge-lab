#pragma once

#include "sourcetrace/core/interfaces.hpp"
#include "sourcetrace/protocol/replay_window.hpp"
#include "sourcetrace/runtime/store.hpp"

#include <cstdint>
#include <string>

namespace sourcetrace::runtime {

enum class RuntimeError : std::uint8_t {
  none,
  storage_failure,
  replay_rejected,
  sequence_conflict,
  stale_configuration,
  unsupported_message,
  command_rejected,
};

struct RuntimeResult {
  bool accepted{false};
  bool duplicate{false};
  RuntimeError error{RuntimeError::none};
  std::string detail;
};

class ControllerRuntime {
public:
  ControllerRuntime(IControllerStore &store, core::ITransport &transport,
                    core::IOutputs &outputs, protocol::DeviceId device_id);

  [[nodiscard]] RuntimeResult handle(const core::Event &event);
  [[nodiscard]] RuntimeResult handleAuthenticatedFrame(const protocol::Frame &frame);
  [[nodiscard]] RuntimeResult flush(std::uint64_t now_ms);

  [[nodiscard]] const core::ControllerState &state() const;
  [[nodiscard]] QueueStats queueStats();
  [[nodiscard]] std::vector<AuditRecord> auditTrail(std::size_t limit);

private:
  struct TelemetryMetadata {
    std::uint64_t sequence;
    std::size_t queue_depth;
  };

  [[nodiscard]] RuntimeResult apply(const core::Event &event,
                                    std::optional<ReplayRecord> replay);
  [[nodiscard]] StoredFrame makeTelemetry(const core::ControllerState &state,
                                          const TelemetryMetadata &metadata) const;
  void applyIndicator();

  IControllerStore &store_;
  core::ITransport &transport_;
  core::IOutputs &outputs_;
  protocol::DeviceId device_id_;
  core::ControllerState state_{};
  std::uint64_t next_outbound_sequence_{1};
  protocol::ReplayWindow replay_;
};

[[nodiscard]] std::string_view runtimeErrorName(RuntimeError error);
[[nodiscard]] std::string_view healthName(core::Health health);
[[nodiscard]] std::string_view modeName(core::ControllerMode mode);
[[nodiscard]] std::string_view indicatorName(core::Indicator indicator);
[[nodiscard]] std::string_view auditName(core::AuditCode code);

} // namespace sourcetrace::runtime
