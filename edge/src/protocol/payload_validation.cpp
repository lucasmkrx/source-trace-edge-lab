#include "sourcetrace/protocol/payload_validation.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <exception>
#include <string_view>

namespace sourcetrace::protocol {
namespace {

using Json = nlohmann::json;

bool boundedJson(const Json &value, std::size_t depth = 0) {
  if (depth > 8) {
    return false;
  }
  if (value.is_string()) {
    return value.get_ref<const std::string &>().size() <= 256;
  }
  if (value.is_array()) {
    if (value.size() > 64) {
      return false;
    }
    for (const auto &item : value) {
      if (!boundedJson(item, depth + 1)) {
        return false;
      }
    }
  }
  if (value.is_object()) {
    if (value.size() > 32) {
      return false;
    }
    for (const auto &[key, item] : value.items()) {
      if (key.size() > 64 || !boundedJson(item, depth + 1)) {
        return false;
      }
    }
  }
  return true;
}

bool exactKeys(const Json &value, std::initializer_list<std::string_view> allowed) {
  for (const auto &[key, unused] : value.items()) {
    (void)unused;
    bool found = false;
    for (const auto candidate : allowed) {
      if (key == candidate) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

PayloadValidation invalid(std::string detail) {
  return PayloadValidation{false, std::move(detail)};
}

} // namespace

PayloadValidation validatePayload(MessageType type, const std::string &payload) {
  Json value;
  try {
    value = Json::parse(payload);
  } catch (const std::exception &) {
    return invalid("payload is not valid JSON");
  }
  if (!value.is_object() || !boundedJson(value)) {
    return invalid("payload must be a bounded JSON object");
  }

  switch (type) {
  case MessageType::registration:
    if (!exactKeys(value, {"firmwareVersion", "capabilities"}) ||
        !value.contains("firmwareVersion") || !value["firmwareVersion"].is_string() ||
        !value.contains("capabilities") || !value["capabilities"].is_array()) {
      return invalid("registration schema is invalid");
    }
    break;
  case MessageType::configuration:
    if (!exactKeys(value, {"configVersion", "telemetryIntervalMs",
                           "occupancyRequiredToStart"}) ||
        !value.contains("configVersion") || !value["configVersion"].is_number_unsigned() ||
        !value.contains("telemetryIntervalMs") ||
        !value["telemetryIntervalMs"].is_number_unsigned() ||
        value["telemetryIntervalMs"].get<std::uint64_t>() < 250U ||
        value["telemetryIntervalMs"].get<std::uint64_t>() > 60'000U ||
        !value.contains("occupancyRequiredToStart") ||
        !value["occupancyRequiredToStart"].is_boolean()) {
      return invalid("configuration schema is invalid");
    }
    break;
  case MessageType::command:
    if (!exactKeys(value, {"commandId", "action"}) || !value.contains("commandId") ||
        !value["commandId"].is_string() ||
        value["commandId"].get_ref<const std::string &>().empty() ||
        value["commandId"].get_ref<const std::string &>().size() > 64 ||
        !value.contains("action") || !value["action"].is_string()) {
      return invalid("command schema is invalid");
    }
    if (const auto &action = value["action"].get_ref<const std::string &>();
        action != "start_session" && action != "stop_session" && action != "request_status") {
      return invalid("command action is unknown");
    }
    break;
  case MessageType::telemetry:
    if (!exactKeys(value, {"eventId", "mode", "health", "occupied", "queueDepth"}) ||
        !value.contains("eventId") || !value["eventId"].is_string() ||
        value["eventId"].get_ref<const std::string &>().empty() ||
        value["eventId"].get_ref<const std::string &>().size() > 64 ||
        !value.contains("mode") || !value["mode"].is_string() || !value.contains("health") ||
        !value["health"].is_string() || !value.contains("occupied") ||
        !value["occupied"].is_boolean() || !value.contains("queueDepth") ||
        !value["queueDepth"].is_number_unsigned()) {
      return invalid("telemetry schema is invalid");
    }
    break;
  case MessageType::acknowledgement:
    if (!exactKeys(value, {"ackSequence", "duplicate"}) || !value.contains("ackSequence") ||
        !value["ackSequence"].is_number_unsigned() || !value.contains("duplicate") ||
        !value["duplicate"].is_boolean()) {
      return invalid("acknowledgement schema is invalid");
    }
    break;
  case MessageType::error:
    if (!exactKeys(value, {"rejectedSequence", "reason"}) ||
        !value.contains("rejectedSequence") ||
        !value["rejectedSequence"].is_number_unsigned() || !value.contains("reason") ||
        !value["reason"].is_string()) {
      return invalid("error schema is invalid");
    }
    break;
  }
  return PayloadValidation{true, {}};
}

} // namespace sourcetrace::protocol
