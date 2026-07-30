#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace sourcetrace::core {

enum class ControllerMode : std::uint8_t {
  booting,
  idle,
  session_active,
};

enum class Health : std::uint8_t {
  nominal,
  network_degraded,
  storage_fault,
};

enum class Indicator : std::uint8_t {
  booting,
  ready,
  active,
  degraded,
  fault,
};

enum class AuditCode : std::uint8_t {
  boot_completed,
  configuration_accepted,
  occupancy_changed,
  session_started,
  session_stopped,
  input_rejected,
  connectivity_lost,
  connectivity_restored,
  storage_failed,
  storage_recovered,
  telemetry_due,
  time_regression,
};

enum class TelemetryReason : std::uint8_t {
  periodic,
  state_changed,
  connectivity_changed,
  fault,
};

enum class RejectionReason : std::uint8_t {
  configuration_required,
  occupancy_required,
  storage_unavailable,
  time_regression,
};

struct ControllerConfig {
  std::uint64_t version{0};
  std::uint64_t telemetry_interval_ms{5'000};
  bool occupancy_required_to_start{true};

  bool operator==(const ControllerConfig &) const = default;
};

struct ControllerState {
  ControllerMode mode{ControllerMode::booting};
  Health health{Health::network_degraded};
  Indicator indicator{Indicator::booting};
  bool occupied{false};
  bool connected{false};
  bool configuration_valid{false};
  ControllerConfig configuration{};
  std::uint64_t monotonic_ms{0};
  std::uint64_t last_telemetry_ms{0};
  std::uint64_t transition_count{0};

  bool operator==(const ControllerState &) const = default;
};

struct BootCompleted {
  bool operator==(const BootCompleted &) const = default;
};

struct OccupancyChanged {
  bool occupied;
  bool operator==(const OccupancyChanged &) const = default;
};

struct SessionButtonPressed {
  bool operator==(const SessionButtonPressed &) const = default;
};

struct ConnectivityChanged {
  bool connected;
  bool operator==(const ConnectivityChanged &) const = default;
};

struct ConfigurationAccepted {
  ControllerConfig configuration;
  bool operator==(const ConfigurationAccepted &) const = default;
};

struct Tick {
  bool operator==(const Tick &) const = default;
};

struct StorageFailed {
  bool operator==(const StorageFailed &) const = default;
};

struct StorageRecovered {
  bool operator==(const StorageRecovered &) const = default;
};

using EventData =
    std::variant<BootCompleted, OccupancyChanged, SessionButtonPressed, ConnectivityChanged,
                 ConfigurationAccepted, Tick, StorageFailed, StorageRecovered>;

struct Event {
  std::uint64_t at_ms;
  EventData data;

  bool operator==(const Event &) const = default;
};

struct SetIndicator {
  Indicator indicator;
  bool operator==(const SetIndicator &) const = default;
};

struct QueueTelemetry {
  TelemetryReason reason;
  bool operator==(const QueueTelemetry &) const = default;
};

struct WriteAudit {
  AuditCode code;
  bool operator==(const WriteAudit &) const = default;
};

struct PersistCheckpoint {
  bool operator==(const PersistCheckpoint &) const = default;
};

struct AttemptFlush {
  bool operator==(const AttemptFlush &) const = default;
};

struct RejectInput {
  RejectionReason reason;
  bool operator==(const RejectInput &) const = default;
};

using EffectData =
    std::variant<SetIndicator, QueueTelemetry, WriteAudit, PersistCheckpoint, AttemptFlush,
                 RejectInput>;

struct Effect {
  EffectData data;
  bool durable{true};

  bool operator==(const Effect &) const = default;
};

struct Transition {
  ControllerState state;
  std::vector<Effect> effects;

  bool operator==(const Transition &) const = default;
};

} // namespace sourcetrace::core
