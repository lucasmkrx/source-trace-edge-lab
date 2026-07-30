#include "sourcetrace/crypto/ed25519.hpp"
#include "sourcetrace/protocol/frame.hpp"
#include "sourcetrace/protocol/parser.hpp"
#include "sourcetrace/runtime/controller_runtime.hpp"
#include "sourcetrace/runtime/sqlite_store.hpp"
#include "sourcetrace/simulator/deterministic_simulator.hpp"
#include "sourcetrace/simulator/fault_injection.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Json = nlohmann::json;
using namespace sourcetrace;

constexpr std::size_t kMaxControlLineBytes = 20'000;

protocol::DeviceId demoDevice() {
  protocol::DeviceId id{};
  for (std::size_t index = 0; index < id.size(); ++index) {
    id[index] = static_cast<std::byte>(index);
  }
  return id;
}

crypto::KeyPair demoKeys() {
  crypto::Seed seed{};
  for (std::size_t index = 0; index < seed.size(); ++index) {
    seed[index] = static_cast<std::byte>(index);
  }
  return crypto::keyPairFromSeed(seed);
}

std::optional<std::vector<std::byte>> fromHex(std::string_view value) {
  if (value.size() % 2 != 0 || value.size() > protocol::kMaxFrameSize * 2) {
    return std::nullopt;
  }
  std::vector<std::byte> bytes(value.size() / 2);
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    unsigned parsed = 0;
    const auto *begin = value.data() + static_cast<std::ptrdiff_t>(index * 2);
    const auto result = std::from_chars(begin, begin + 2, parsed, 16);
    if (result.ec != std::errc{} || result.ptr != begin + 2 || parsed > 0xFFU) {
      return std::nullopt;
    }
    bytes[index] = static_cast<std::byte>(parsed);
  }
  return bytes;
}

Json stateJson(const core::ControllerState &state) {
  return Json{
      {"mode", runtime::modeName(state.mode)},
      {"health", runtime::healthName(state.health)},
      {"indicator", runtime::indicatorName(state.indicator)},
      {"occupied", state.occupied},
      {"connected", state.connected},
      {"configurationValid", state.configuration_valid},
      {"configurationVersion", state.configuration.version},
      {"monotonicMs", state.monotonic_ms},
      {"lastTelemetryMs", state.last_telemetry_ms},
      {"transitionCount", state.transition_count},
  };
}

Json runtimeResultJson(const runtime::RuntimeResult &result) {
  return Json{
      {"accepted", result.accepted},
      {"duplicate", result.duplicate},
      {"error", runtime::runtimeErrorName(result.error)},
      {"detail", result.detail},
  };
}

class Application {
public:
  Application(const std::filesystem::path &database_path, runtime::QueueLimits limits)
      : durable_store_(database_path, limits), store_(durable_store_),
        verifier_(demoKeys().public_key) {
    restart();
  }

  Json execute(const Json &request) {
    const auto action = request.at("action").get<std::string>();
    Json outcome{{"accepted", true},
                 {"duplicate", false},
                 {"error", "none"},
                 {"detail", "accepted"}};

    if (action == "status") {
      outcome["detail"] = "status";
    } else if (action == "boot") {
      outcome = runtimeResultJson(
          controller().handle(core::Event{atMs(request), core::BootCompleted{}}));
    } else if (action == "occupancy") {
      outcome = runtimeResultJson(controller().handle(
          core::Event{atMs(request), core::OccupancyChanged{request.at("occupied").get<bool>()}}));
    } else if (action == "button") {
      outcome = runtimeResultJson(
          controller().handle(core::Event{atMs(request), core::SessionButtonPressed{}}));
    } else if (action == "network") {
      const auto connected = request.at("connected").get<bool>();
      transport_.setConnected(connected);
      outcome = runtimeResultJson(controller().handle(
          core::Event{atMs(request), core::ConnectivityChanged{connected}}));
    } else if (action == "tick") {
      outcome =
          runtimeResultJson(controller().handle(core::Event{atMs(request), core::Tick{}}));
    } else if (action == "storageRecovered") {
      outcome = runtimeResultJson(
          controller().handle(core::Event{atMs(request), core::StorageRecovered{}}));
    } else if (action == "flush") {
      outcome = runtimeResultJson(controller().flush(atMs(request)));
    } else if (action == "faultSends") {
      transport_.failNextSends(request.at("count").get<std::size_t>());
      outcome["detail"] = "transport send fault armed";
    } else if (action == "faultCommits") {
      store_.failNextCommits(request.at("count").get<std::size_t>());
      outcome["detail"] = "storage commit fault armed";
    } else if (action == "faultAcks") {
      store_.failNextAcknowledgements(request.at("count").get<std::size_t>());
      outcome["detail"] = "storage acknowledgement fault armed";
    } else if (action == "restart") {
      restart();
      outcome["detail"] = "runtime reconstructed from durable state";
    } else if (action == "injectFrame") {
      outcome = injectFrame(request.at("frameHex").get<std::string>());
    } else if (action == "noiseSample") {
      simulator::DeterministicNoise noise(request.at("seed").get<std::uint32_t>());
      outcome["sample"] = noise.occupancySample(
          request.at("occupied").get<bool>(), request.at("noisePercent").get<std::uint8_t>());
      outcome["detail"] = "deterministic sample";
    } else {
      throw std::invalid_argument("unknown action");
    }
    return response(std::move(outcome));
  }

private:
  static std::uint64_t atMs(const Json &request) {
    return request.at("atMs").get<std::uint64_t>();
  }

  runtime::ControllerRuntime &controller() {
    if (!controller_) {
      throw std::logic_error("controller is unavailable");
    }
    return *controller_;
  }

  void restart() {
    controller_ = std::make_unique<runtime::ControllerRuntime>(
        store_, transport_, hardware_, demoDevice());
  }

  Json injectFrame(const std::string &hex) {
    const auto bytes = fromHex(hex);
    if (!bytes) {
      return Json{{"accepted", false},
                  {"duplicate", false},
                  {"error", "invalid_hex"},
                  {"detail", "frameHex must be bounded, even-length hexadecimal"}};
    }
    protocol::StreamParser parser(&verifier_);
    auto parsed = parser.feed(*bytes);
    if (parsed.consumed != bytes->size() || parsed.results.size() != 1 ||
        !parsed.results.front().ok()) {
      const auto detail = parsed.results.empty() ? "incomplete frame"
                                                  : parsed.results.front().detail;
      const auto code =
          parsed.results.empty()
              ? static_cast<std::uint16_t>(protocol::ErrorCode::invalid_length)
              : static_cast<std::uint16_t>(parsed.results.front().error);
      return Json{{"accepted", false},
                  {"duplicate", false},
                  {"error", "protocol_rejected"},
                  {"protocolErrorCode", code},
                  {"detail", detail}};
    }
    const auto frame = parsed.results.front().frame.value_or(protocol::Frame{});
    return runtimeResultJson(controller().handleAuthenticatedFrame(frame));
  }

  Json response(Json outcome) {
    outcome["controlVersion"] = 1;
    outcome["kind"] = "result";
    outcome["state"] = stateJson(controller().state());
    const auto queue = controller().queueStats();
    outcome["queue"] = {{"records", queue.records}, {"bytes", queue.bytes}};
    outcome["receiverAppliedEffects"] = transport_.appliedEffects();

    auto &frames = outcome["transmittedFramesHex"] = Json::array();
    const auto &transmissions = transport_.transmissions();
    for (; emitted_transmissions_ < transmissions.size(); ++emitted_transmissions_) {
      frames.push_back(protocol::toHex(transmissions[emitted_transmissions_]));
    }

    auto &audits = outcome["auditTail"] = Json::array();
    for (const auto &audit : controller().auditTrail(16)) {
      audits.push_back(Json{{"atMs", audit.at_ms},
                            {"code", runtime::auditName(audit.code)},
                            {"detail", audit.detail}});
    }
    return outcome;
  }

  runtime::SQLiteStore durable_store_;
  simulator::FaultInjectingStore store_;
  simulator::SimulatedTransport transport_;
  simulator::SimulatedHardware hardware_;
  crypto::Ed25519Verifier verifier_;
  std::unique_ptr<runtime::ControllerRuntime> controller_;
  std::size_t emitted_transmissions_{0};
};

struct Options {
  std::filesystem::path database{":memory:"};
  runtime::QueueLimits limits{};
};

std::size_t parseSize(std::string_view value) {
  std::size_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed == 0) {
    throw std::invalid_argument("size option must be a positive integer");
  }
  return parsed;
}

Options parseOptions(int argc, char **argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--db" && index + 1 < argc) {
      options.database = argv[++index];
    } else if (argument == "--queue-records" && index + 1 < argc) {
      options.limits.max_records = parseSize(argv[++index]);
    } else if (argument == "--queue-bytes" && index + 1 < argc) {
      options.limits.max_bytes = parseSize(argv[++index]);
    } else {
      throw std::invalid_argument(
          "usage: sourcetrace_sim [--db PATH] [--queue-records N] [--queue-bytes N]");
    }
  }
  return options;
}

Json inputFailure(std::string detail) {
  return Json{{"controlVersion", 1},
              {"kind", "result"},
              {"accepted", false},
              {"duplicate", false},
              {"error", "invalid_control_input"},
              {"detail", std::move(detail)}};
}

} // namespace

int main(int argc, char **argv) {
  try {
    const auto options = parseOptions(argc, argv);
    Application application(options.database, options.limits);
    std::string line;
    while (std::getline(std::cin, line)) {
      if (line.size() > kMaxControlLineBytes) {
        std::cout << inputFailure("control line exceeds 20000 bytes").dump() << '\n';
        continue;
      }
      try {
        const auto request = Json::parse(line);
        std::cout << application.execute(request).dump() << '\n';
      } catch (const std::exception &error) {
        std::cout << inputFailure(error.what()).dump() << '\n';
      }
      std::cout.flush();
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 2;
  }
  return 0;
}
