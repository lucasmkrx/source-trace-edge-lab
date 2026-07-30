#include "sourcetrace/runtime/controller_runtime.hpp"

#include "sourcetrace/core/reducer.hpp"
#include "sourcetrace/protocol/frame.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <variant>

namespace sourcetrace::runtime {
namespace {

bool hasRejection(const std::vector<core::Effect> &effects) {
  return std::any_of(effects.begin(), effects.end(), [](const core::Effect &effect) {
    return std::holds_alternative<core::RejectInput>(effect.data);
  });
}

std::uint64_t retryDelay(std::uint32_t attempt) {
  static constexpr std::array<std::uint64_t, 5> delays{250, 500, 1'000, 2'000, 4'000};
  return delays[std::min<std::size_t>(attempt, delays.size() - 1)];
}

} // namespace

ControllerRuntime::ControllerRuntime(IControllerStore &store, core::ITransport &transport,
                                     core::IOutputs &outputs,
                                     protocol::DeviceId device_id)
    : store_(store), transport_(transport), outputs_(outputs), device_id_(device_id) {
  const auto recovery = store_.recover();
  if (recovery.state) {
    state_ = *recovery.state;
  }
  next_outbound_sequence_ = recovery.next_outbound_sequence;
  if (recovery.replay) {
    replay_.restore(recovery.replay->sequence, recovery.replay->signature);
  }
  applyIndicator();
}

RuntimeResult ControllerRuntime::handle(const core::Event &event) {
  const bool reconnect =
      std::holds_alternative<core::ConnectivityChanged>(event.data) &&
      std::get<core::ConnectivityChanged>(event.data).connected;
  auto result = apply(event, std::nullopt);
  if (result.accepted && reconnect) {
    if (store_.resetRetryBudget() != StoreStatus::ok) {
      return RuntimeResult{false, false, RuntimeError::storage_failure,
                           "could not reset the bounded retry budget"};
    }
    return flush(event.at_ms);
  }
  return result;
}

RuntimeResult ControllerRuntime::apply(const core::Event &event,
                                       std::optional<ReplayRecord> replay_record) {
  auto transition = core::reduce(state_, event);
  CommitBatch batch;
  batch.state = transition.state;
  batch.next_outbound_sequence = next_outbound_sequence_;
  batch.replay = replay_record;

  bool wants_telemetry = false;
  for (const auto &effect : transition.effects) {
    if (const auto *audit = std::get_if<core::WriteAudit>(&effect.data);
        audit != nullptr && effect.durable) {
      batch.audits.push_back(AuditRecord{event.at_ms, audit->code,
                                         std::string(auditName(audit->code))});
    }
    if (std::holds_alternative<core::QueueTelemetry>(effect.data) && effect.durable) {
      wants_telemetry = true;
    }
  }

  if (wants_telemetry) {
    const auto stats = store_.queueStats();
    auto queued = makeTelemetry(
        transition.state, TelemetryMetadata{next_outbound_sequence_, stats.records + 1});
    if (store_.hasCapacity(1, queued.bytes.size())) {
      batch.outbound.push_back(std::move(queued));
      ++batch.next_outbound_sequence;
    } else {
      const auto saturated =
          core::reduce(transition.state, core::Event{event.at_ms, core::QueueCapacityReached{}});
      batch.state = saturated.state;
      for (const auto &effect : saturated.effects) {
        if (const auto *audit = std::get_if<core::WriteAudit>(&effect.data);
            audit != nullptr && effect.durable) {
          batch.audits.push_back(AuditRecord{event.at_ms, audit->code,
                                             std::string(auditName(audit->code))});
        }
      }
    }
  }

  auto status = store_.commit(batch);
  if (status == StoreStatus::queue_full && !batch.outbound.empty()) {
    batch.outbound.clear();
    const auto saturated =
        core::reduce(batch.state, core::Event{event.at_ms, core::QueueCapacityReached{}});
    batch.state = saturated.state;
    batch.next_outbound_sequence = next_outbound_sequence_;
    batch.audits.push_back(AuditRecord{event.at_ms, core::AuditCode::queue_saturated,
                                       "queue_saturated"});
    status = store_.commit(batch);
  }
  if (status != StoreStatus::ok) {
    const auto failed =
        core::reduce(state_, core::Event{event.at_ms, core::StorageFailed{}});
    state_ = failed.state;
    applyIndicator();
    return RuntimeResult{false, false, RuntimeError::storage_failure,
                         "atomic local commit failed; transition was not accepted"};
  }

  state_ = batch.state;
  next_outbound_sequence_ = batch.next_outbound_sequence;
  if (replay_record) {
    protocol::Frame replay_frame;
    replay_frame.sequence = replay_record->sequence;
    replay_frame.signature = replay_record->signature;
    replay_.commit(replay_frame);
  }
  applyIndicator();
  return RuntimeResult{!hasRejection(transition.effects), false,
                       hasRejection(transition.effects) ? RuntimeError::command_rejected
                                                        : RuntimeError::none,
                       hasRejection(transition.effects) ? "domain policy rejected the input"
                                                        : "accepted"};
}

RuntimeResult ControllerRuntime::handleAuthenticatedFrame(const protocol::Frame &frame) {
  if (!frame.signature) {
    return RuntimeResult{false, false, RuntimeError::unsupported_message,
                         "runtime accepts only previously authenticated frames"};
  }
  const auto replay_decision = replay_.evaluate(frame);
  if (replay_decision == protocol::ReplayDecision::duplicate) {
    return RuntimeResult{true, true, RuntimeError::none, "duplicate acknowledged without effect"};
  }
  if (replay_decision == protocol::ReplayDecision::replay) {
    return RuntimeResult{false, false, RuntimeError::replay_rejected,
                         "sequence is below the durable high-water mark"};
  }
  if (replay_decision == protocol::ReplayDecision::conflict) {
    return RuntimeResult{false, false, RuntimeError::sequence_conflict,
                         "sequence was reused with different authenticated content"};
  }

  const auto payload = nlohmann::json::parse(frame.payload);
  core::Event event{frame.monotonic_ms, core::Tick{}};
  if (frame.type == protocol::MessageType::configuration) {
    const auto version = payload.at("configVersion").get<std::uint64_t>();
    if (state_.configuration_valid && version <= state_.configuration.version) {
      return RuntimeResult{false, false, RuntimeError::stale_configuration,
                           "configuration version must increase"};
    }
    event.data = core::ConfigurationAccepted{core::ControllerConfig{
        version,
        payload.at("telemetryIntervalMs").get<std::uint64_t>(),
        payload.at("occupancyRequiredToStart").get<bool>(),
    }};
  } else if (frame.type == protocol::MessageType::command) {
    const auto action = payload.at("action").get<std::string>();
    core::CommandAction command = core::CommandAction::request_status;
    if (action == "start_session") {
      command = core::CommandAction::start_session;
    } else if (action == "stop_session") {
      command = core::CommandAction::stop_session;
    }
    event.data = core::RemoteCommand{command};
  } else {
    return RuntimeResult{false, false, RuntimeError::unsupported_message,
                         "authenticated runtime input must be configuration or command"};
  }
  return apply(event, ReplayRecord{frame.sequence, *frame.signature});
}

StoredFrame ControllerRuntime::makeTelemetry(const core::ControllerState &state,
                                             const TelemetryMetadata &metadata) const {
  const auto event_id = "evt-" + std::to_string(metadata.sequence);
  const nlohmann::json payload{
      {"eventId", event_id},
      {"health", healthName(state.health)},
      {"mode", modeName(state.mode)},
      {"occupied", state.occupied},
      {"queueDepth", metadata.queue_depth},
  };
  protocol::Frame frame{
      protocol::MessageType::telemetry,
      0,
      protocol::ErrorCode::none,
      device_id_,
      metadata.sequence,
      state.monotonic_ms,
      payload.dump(),
      std::nullopt,
  };
  auto encoded = protocol::encode(frame);
  if (!encoded.ok()) {
    throw std::runtime_error("internally generated telemetry violated the protocol schema");
  }
  return StoredFrame{event_id, metadata.sequence, std::move(encoded.bytes), 0, 0};
}

RuntimeResult ControllerRuntime::flush(std::uint64_t now_ms) {
  if (!transport_.connected()) {
    return RuntimeResult{true, false, RuntimeError::none, "offline; events remain queued"};
  }
  const auto frames = store_.pending(32);
  for (const auto &frame : frames) {
    if (frame.attempts >= 5 || frame.next_retry_ms > now_ms) {
      continue;
    }
    if (!transport_.send(frame.bytes)) {
      const auto attempts = frame.attempts + 1;
      if (store_.recordAttempt(
              frame.event_id,
              RetryState{attempts, now_ms + retryDelay(frame.attempts)}) != StoreStatus::ok) {
        return RuntimeResult{false, false, RuntimeError::storage_failure,
                             "could not persist retry state"};
      }
      break;
    }
    if (store_.acknowledge(frame.event_id) != StoreStatus::ok) {
      return RuntimeResult{false, false, RuntimeError::storage_failure,
                           "delivery succeeded but acknowledgement was not persisted"};
    }
  }

  if (state_.health == core::Health::queue_saturated && store_.hasCapacity(1, 1)) {
    return apply(core::Event{now_ms, core::QueueCapacityAvailable{}}, std::nullopt);
  }
  return RuntimeResult{true, false, RuntimeError::none, "flush complete"};
}

const core::ControllerState &ControllerRuntime::state() const { return state_; }

QueueStats ControllerRuntime::queueStats() { return store_.queueStats(); }

std::vector<AuditRecord> ControllerRuntime::auditTrail(std::size_t limit) {
  return store_.auditTrail(limit);
}

void ControllerRuntime::applyIndicator() { outputs_.setIndicator(state_.indicator); }

std::string_view runtimeErrorName(RuntimeError error) {
  switch (error) {
  case RuntimeError::none:
    return "none";
  case RuntimeError::storage_failure:
    return "storage_failure";
  case RuntimeError::replay_rejected:
    return "replay_rejected";
  case RuntimeError::sequence_conflict:
    return "sequence_conflict";
  case RuntimeError::stale_configuration:
    return "stale_configuration";
  case RuntimeError::unsupported_message:
    return "unsupported_message";
  case RuntimeError::command_rejected:
    return "command_rejected";
  }
  return "unknown";
}

std::string_view healthName(core::Health health) {
  switch (health) {
  case core::Health::nominal:
    return "nominal";
  case core::Health::network_degraded:
    return "network_degraded";
  case core::Health::queue_saturated:
    return "queue_saturated";
  case core::Health::storage_fault:
    return "storage_fault";
  }
  return "unknown";
}

std::string_view modeName(core::ControllerMode mode) {
  switch (mode) {
  case core::ControllerMode::booting:
    return "booting";
  case core::ControllerMode::idle:
    return "idle";
  case core::ControllerMode::session_active:
    return "session_active";
  }
  return "unknown";
}

std::string_view indicatorName(core::Indicator indicator) {
  switch (indicator) {
  case core::Indicator::booting:
    return "booting";
  case core::Indicator::ready:
    return "ready";
  case core::Indicator::active:
    return "active";
  case core::Indicator::degraded:
    return "degraded";
  case core::Indicator::fault:
    return "fault";
  }
  return "unknown";
}

std::string_view auditName(core::AuditCode code) {
  switch (code) {
  case core::AuditCode::boot_completed:
    return "boot_completed";
  case core::AuditCode::configuration_accepted:
    return "configuration_accepted";
  case core::AuditCode::occupancy_changed:
    return "occupancy_changed";
  case core::AuditCode::session_started:
    return "session_started";
  case core::AuditCode::session_stopped:
    return "session_stopped";
  case core::AuditCode::input_rejected:
    return "input_rejected";
  case core::AuditCode::connectivity_lost:
    return "connectivity_lost";
  case core::AuditCode::connectivity_restored:
    return "connectivity_restored";
  case core::AuditCode::storage_failed:
    return "storage_failed";
  case core::AuditCode::storage_recovered:
    return "storage_recovered";
  case core::AuditCode::queue_saturated:
    return "queue_saturated";
  case core::AuditCode::queue_available:
    return "queue_available";
  case core::AuditCode::remote_command_accepted:
    return "remote_command_accepted";
  case core::AuditCode::telemetry_due:
    return "telemetry_due";
  case core::AuditCode::time_regression:
    return "time_regression";
  }
  return "unknown";
}

} // namespace sourcetrace::runtime
