#include "sourcetrace/core/reducer.hpp"

#include <type_traits>

namespace sourcetrace::core {
namespace {

void pushAudit(std::vector<Effect> &effects, AuditCode code, bool durable = true) {
  effects.push_back(Effect{WriteAudit{code}, durable});
}

void pushCheckpoint(std::vector<Effect> &effects, const ControllerState &state) {
  if (state.health != Health::storage_fault) {
    effects.push_back(Effect{PersistCheckpoint{}, true});
  }
}

void applyIndicator(ControllerState &state, std::vector<Effect> &effects) {
  const auto desired = desiredIndicator(state);
  if (desired != state.indicator) {
    state.indicator = desired;
    effects.push_back(Effect{SetIndicator{desired}, false});
  }
}

} // namespace

Indicator desiredIndicator(const ControllerState &state) {
  if (state.mode == ControllerMode::booting) {
    return Indicator::booting;
  }
  if (state.health == Health::storage_fault) {
    return Indicator::fault;
  }
  if (state.health == Health::network_degraded || state.health == Health::queue_saturated ||
      !state.configuration_valid) {
    return Indicator::degraded;
  }
  if (state.mode == ControllerMode::session_active) {
    return Indicator::active;
  }
  return Indicator::ready;
}

Transition reduce(const ControllerState &current, const Event &event) {
  Transition result{current, {}};
  if (event.at_ms < current.monotonic_ms) {
    result.effects.push_back(Effect{RejectInput{RejectionReason::time_regression}, false});
    pushAudit(result.effects, AuditCode::time_regression, false);
    return result;
  }

  result.state.monotonic_ms = event.at_ms;
  bool changed = false;

  std::visit(
      [&](const auto &data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, BootCompleted>) {
          if (result.state.mode == ControllerMode::booting) {
            result.state.mode = ControllerMode::idle;
            changed = true;
            pushAudit(result.effects, AuditCode::boot_completed);
          }
        } else if constexpr (std::is_same_v<T, OccupancyChanged>) {
          if (result.state.occupied != data.occupied) {
            result.state.occupied = data.occupied;
            changed = true;
            pushAudit(result.effects, AuditCode::occupancy_changed);
            result.effects.push_back(Effect{QueueTelemetry{TelemetryReason::state_changed}, true});
          }
        } else if constexpr (std::is_same_v<T, SessionButtonPressed>) {
          if (result.state.mode == ControllerMode::session_active) {
            result.state.mode = ControllerMode::idle;
            changed = true;
            pushAudit(result.effects, AuditCode::session_stopped,
                      result.state.health != Health::storage_fault);
            result.effects.push_back(Effect{QueueTelemetry{TelemetryReason::state_changed},
                                            result.state.health != Health::storage_fault});
          } else if (!result.state.configuration_valid) {
            result.effects.push_back(
                Effect{RejectInput{RejectionReason::configuration_required}, false});
            pushAudit(result.effects, AuditCode::input_rejected,
                      result.state.health != Health::storage_fault);
          } else if (result.state.configuration.occupancy_required_to_start &&
                     !result.state.occupied) {
            result.effects.push_back(
                Effect{RejectInput{RejectionReason::occupancy_required}, false});
            pushAudit(result.effects, AuditCode::input_rejected,
                      result.state.health != Health::storage_fault);
          } else if (result.state.health == Health::storage_fault) {
            result.effects.push_back(
                Effect{RejectInput{RejectionReason::storage_unavailable}, false});
            pushAudit(result.effects, AuditCode::input_rejected, false);
          } else if (result.state.health == Health::queue_saturated) {
            result.effects.push_back(
                Effect{RejectInput{RejectionReason::queue_unavailable}, false});
            pushAudit(result.effects, AuditCode::input_rejected);
          } else {
            result.state.mode = ControllerMode::session_active;
            changed = true;
            pushAudit(result.effects, AuditCode::session_started);
            result.effects.push_back(Effect{QueueTelemetry{TelemetryReason::state_changed}, true});
          }
        } else if constexpr (std::is_same_v<T, ConnectivityChanged>) {
          if (result.state.connected != data.connected) {
            result.state.connected = data.connected;
            if (result.state.health != Health::storage_fault &&
                result.state.health != Health::queue_saturated) {
              result.state.health =
                  data.connected ? Health::nominal : Health::network_degraded;
            }
            changed = true;
            pushAudit(result.effects, data.connected ? AuditCode::connectivity_restored
                                                     : AuditCode::connectivity_lost);
            result.effects.push_back(
                Effect{QueueTelemetry{TelemetryReason::connectivity_changed}, true});
            if (data.connected) {
              result.effects.push_back(Effect{AttemptFlush{}, false});
            }
          }
        } else if constexpr (std::is_same_v<T, ConfigurationAccepted>) {
          if (!result.state.configuration_valid ||
              data.configuration.version > result.state.configuration.version) {
            result.state.configuration = data.configuration;
            result.state.configuration_valid = true;
            changed = true;
            pushAudit(result.effects, AuditCode::configuration_accepted);
            result.effects.push_back(Effect{QueueTelemetry{TelemetryReason::state_changed}, true});
          }
        } else if constexpr (std::is_same_v<T, Tick>) {
          if (result.state.configuration_valid &&
              event.at_ms - result.state.last_telemetry_ms >=
                  result.state.configuration.telemetry_interval_ms) {
            result.state.last_telemetry_ms = event.at_ms;
            changed = true;
            pushAudit(result.effects, AuditCode::telemetry_due);
            result.effects.push_back(Effect{QueueTelemetry{TelemetryReason::periodic}, true});
          }
        } else if constexpr (std::is_same_v<T, StorageFailed>) {
          if (result.state.health != Health::storage_fault) {
            result.state.health = Health::storage_fault;
            changed = true;
            pushAudit(result.effects, AuditCode::storage_failed, false);
            result.effects.push_back(Effect{QueueTelemetry{TelemetryReason::fault}, false});
          }
        } else if constexpr (std::is_same_v<T, StorageRecovered>) {
          if (result.state.health == Health::storage_fault) {
            result.state.health =
                result.state.connected ? Health::nominal : Health::network_degraded;
            changed = true;
            pushAudit(result.effects, AuditCode::storage_recovered);
            result.effects.push_back(Effect{QueueTelemetry{TelemetryReason::fault}, true});
          }
        } else if constexpr (std::is_same_v<T, QueueCapacityReached>) {
          if (result.state.health != Health::storage_fault &&
              result.state.health != Health::queue_saturated) {
            result.state.health = Health::queue_saturated;
            changed = true;
            pushAudit(result.effects, AuditCode::queue_saturated);
          }
        } else if constexpr (std::is_same_v<T, QueueCapacityAvailable>) {
          if (result.state.health == Health::queue_saturated) {
            result.state.health =
                result.state.connected ? Health::nominal : Health::network_degraded;
            changed = true;
            pushAudit(result.effects, AuditCode::queue_available);
          }
        } else if constexpr (std::is_same_v<T, RemoteCommand>) {
          if (data.action == CommandAction::request_status) {
            pushAudit(result.effects, AuditCode::remote_command_accepted,
                      result.state.health != Health::storage_fault);
            result.effects.push_back(Effect{QueueTelemetry{TelemetryReason::state_changed},
                                            result.state.health != Health::storage_fault});
          } else if (data.action == CommandAction::start_session) {
            if (result.state.mode == ControllerMode::session_active) {
              pushAudit(result.effects, AuditCode::remote_command_accepted,
                        result.state.health != Health::storage_fault);
            } else if (!result.state.configuration_valid) {
              result.effects.push_back(
                  Effect{RejectInput{RejectionReason::configuration_required}, false});
              pushAudit(result.effects, AuditCode::input_rejected,
                        result.state.health != Health::storage_fault);
            } else if (result.state.configuration.occupancy_required_to_start &&
                       !result.state.occupied) {
              result.effects.push_back(
                  Effect{RejectInput{RejectionReason::occupancy_required}, false});
              pushAudit(result.effects, AuditCode::input_rejected,
                        result.state.health != Health::storage_fault);
            } else if (result.state.health == Health::storage_fault) {
              result.effects.push_back(
                  Effect{RejectInput{RejectionReason::storage_unavailable}, false});
              pushAudit(result.effects, AuditCode::input_rejected, false);
            } else if (result.state.health == Health::queue_saturated) {
              result.effects.push_back(
                  Effect{RejectInput{RejectionReason::queue_unavailable}, false});
              pushAudit(result.effects, AuditCode::input_rejected);
            } else {
              result.state.mode = ControllerMode::session_active;
              changed = true;
              pushAudit(result.effects, AuditCode::remote_command_accepted);
              result.effects.push_back(
                  Effect{QueueTelemetry{TelemetryReason::state_changed}, true});
            }
          } else {
            if (result.state.mode == ControllerMode::session_active) {
              result.state.mode = ControllerMode::idle;
              changed = true;
              result.effects.push_back(Effect{QueueTelemetry{TelemetryReason::state_changed},
                                              result.state.health != Health::storage_fault});
            }
            pushAudit(result.effects, AuditCode::remote_command_accepted,
                      result.state.health != Health::storage_fault);
          }
        }
      },
      event.data);

  applyIndicator(result.state, result.effects);
  if (changed) {
    ++result.state.transition_count;
    pushCheckpoint(result.effects, result.state);
  }
  return result;
}

} // namespace sourcetrace::core
