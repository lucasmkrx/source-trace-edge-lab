#include "sourcetrace/crypto/ed25519.hpp"
#include "sourcetrace/protocol/frame.hpp"
#include "sourcetrace/runtime/controller_runtime.hpp"
#include "sourcetrace/runtime/sqlite_store.hpp"
#include "sourcetrace/simulator/deterministic_simulator.hpp"
#include "sourcetrace/simulator/fault_injection.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace {

using namespace sourcetrace;

class TestDatabase {
public:
  TestDatabase() {
    static std::atomic_uint64_t counter{0};
    path_ = std::filesystem::temp_directory_path() /
            ("sourcetrace-runtime-" + std::to_string(++counter) + ".sqlite3");
    std::filesystem::remove(path_);
  }

  ~TestDatabase() {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
    std::filesystem::remove(path_.string() + "-journal", ignored);
  }

  [[nodiscard]] const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

protocol::DeviceId testDevice() {
  protocol::DeviceId id{};
  for (std::size_t index = 0; index < id.size(); ++index) {
    id[index] = static_cast<std::byte>(index);
  }
  return id;
}

crypto::KeyPair testKeys() {
  crypto::Seed seed{};
  for (std::size_t index = 0; index < seed.size(); ++index) {
    seed[index] = static_cast<std::byte>(index);
  }
  return crypto::keyPairFromSeed(seed);
}

protocol::Frame signedConfiguration(std::uint64_t sequence, std::uint64_t version = 1) {
  auto frame = protocol::Frame{
      protocol::MessageType::configuration,
      protocol::kFlagAuthenticated,
      protocol::ErrorCode::none,
      testDevice(),
      sequence,
      sequence * 10,
      "{\"configVersion\":" + std::to_string(version) +
          ",\"occupancyRequiredToStart\":true,\"telemetryIntervalMs\":1000}",
      std::nullopt,
  };
  const auto keys = testKeys();
  frame.signature =
      crypto::signDetached(protocol::authenticationMessage(frame), keys.secret_key);
  return frame;
}

void bootstrap(runtime::ControllerRuntime &controller) {
  REQUIRE(controller.handle(core::Event{1, core::BootCompleted{}}).accepted);
  REQUIRE(controller
              .handle(core::Event{2, core::ConfigurationAccepted{
                                            core::ControllerConfig{1, 1'000, true}}})
              .accepted);
}

} // namespace

TEST_CASE("restart reconstructs the durable checkpoint sequence and pending queue") {
  TestDatabase database;
  {
    runtime::SQLiteStore store(database.path());
    simulator::SimulatedTransport transport;
    simulator::SimulatedHardware hardware;
    runtime::ControllerRuntime controller(store, transport, hardware, testDevice());
    bootstrap(controller);
    REQUIRE(controller.handle(core::Event{3, core::OccupancyChanged{true}}).accepted);
    CHECK(controller.state().occupied);
    CHECK(controller.queueStats().records == 2);
  }

  runtime::SQLiteStore recovered_store(database.path());
  simulator::SimulatedTransport recovered_transport;
  simulator::SimulatedHardware recovered_hardware;
  runtime::ControllerRuntime recovered(recovered_store, recovered_transport,
                                       recovered_hardware, testDevice());
  CHECK(recovered.state().configuration_valid);
  CHECK(recovered.state().occupied);
  CHECK(recovered.state().transition_count == 3);
  CHECK(recovered.queueStats().records == 2);
  CHECK(recovered_store.recover().next_outbound_sequence == 3);
}

TEST_CASE("reconnection flushes queued frames in sequence order") {
  TestDatabase database;
  runtime::SQLiteStore store(database.path());
  simulator::SimulatedTransport transport;
  simulator::SimulatedHardware hardware;
  runtime::ControllerRuntime controller(store, transport, hardware, testDevice());
  bootstrap(controller);
  REQUIRE(controller.handle(core::Event{3, core::OccupancyChanged{true}}).accepted);

  transport.setConnected(true);
  const auto result =
      controller.handle(core::Event{4, core::ConnectivityChanged{true}});
  CHECK(result.accepted);
  CHECK(controller.queueStats().records == 0);
  CHECK(transport.transmissions().size() == 3);
  CHECK(transport.appliedEffects() == 3);
}

TEST_CASE("lost local acknowledgement retransmits without duplicating receiver effect") {
  TestDatabase database;
  runtime::SQLiteStore durable_store(database.path());
  simulator::FaultInjectingStore store(durable_store);
  simulator::SimulatedTransport transport;
  simulator::SimulatedHardware hardware;
  runtime::ControllerRuntime controller(store, transport, hardware, testDevice());
  bootstrap(controller);
  transport.setConnected(true);
  store.failNextAcknowledgements(1);

  const auto first = controller.flush(10);
  CHECK_FALSE(first.accepted);
  CHECK(first.error == runtime::RuntimeError::storage_failure);
  CHECK(controller.queueStats().records == 1);
  CHECK(transport.appliedEffects() == 1);

  CHECK(controller.flush(11).accepted);
  CHECK(controller.queueStats().records == 0);
  CHECK(transport.transmissions().size() == 2);
  CHECK(transport.appliedEffects() == 1);
}

TEST_CASE("retry policy stops after the persisted five-attempt budget") {
  TestDatabase database;
  runtime::SQLiteStore store(database.path());
  simulator::SimulatedTransport transport;
  simulator::SimulatedHardware hardware;
  runtime::ControllerRuntime controller(store, transport, hardware, testDevice());
  bootstrap(controller);
  transport.setConnected(true);
  transport.failNextSends(10);

  CHECK(controller.flush(0).accepted);
  CHECK(controller.flush(250).accepted);
  CHECK(controller.flush(750).accepted);
  CHECK(controller.flush(1'750).accepted);
  CHECK(controller.flush(3'750).accepted);
  CHECK(controller.flush(7'750).accepted);

  REQUIRE(controller.queueStats().records == 1);
  const auto pending = store.pending(1);
  REQUIRE(pending.size() == 1);
  CHECK(pending.front().attempts == 5);
  CHECK(transport.transmissions().size() == 5);
}

TEST_CASE("queue saturation is durable and blocks a new session") {
  TestDatabase database;
  runtime::SQLiteStore store(database.path(), runtime::QueueLimits{1, 1'024U * 1'024U});
  simulator::SimulatedTransport transport;
  simulator::SimulatedHardware hardware;
  runtime::ControllerRuntime controller(store, transport, hardware, testDevice());
  bootstrap(controller);
  REQUIRE(controller.queueStats().records == 1);

  REQUIRE(controller.handle(core::Event{3, core::OccupancyChanged{true}}).accepted);
  CHECK(controller.state().health == core::Health::queue_saturated);
  CHECK(controller.state().occupied);
  CHECK(controller.queueStats().records == 1);

  const auto start = controller.handle(core::Event{4, core::SessionButtonPressed{}});
  CHECK_FALSE(start.accepted);
  CHECK(start.error == runtime::RuntimeError::command_rejected);
  CHECK(controller.state().mode == core::ControllerMode::idle);
}

TEST_CASE("failed transaction changes only volatile health and recovery returns prior state") {
  TestDatabase database;
  runtime::SQLiteStore durable_store(database.path());
  simulator::FaultInjectingStore store(durable_store);
  simulator::SimulatedTransport transport;
  simulator::SimulatedHardware hardware;
  runtime::ControllerRuntime controller(store, transport, hardware, testDevice());
  bootstrap(controller);
  store.failNextCommits(1);

  const auto failed = controller.handle(core::Event{3, core::OccupancyChanged{true}});
  CHECK_FALSE(failed.accepted);
  CHECK(failed.error == runtime::RuntimeError::storage_failure);
  CHECK(controller.state().health == core::Health::storage_fault);
  CHECK_FALSE(controller.state().occupied);

  simulator::SimulatedHardware recovered_hardware;
  runtime::ControllerRuntime recovered(durable_store, transport, recovered_hardware,
                                       testDevice());
  CHECK(recovered.state().health == core::Health::network_degraded);
  CHECK_FALSE(recovered.state().occupied);
  CHECK(recovered.queueStats().records == 1);
}

TEST_CASE("authenticated replay state survives restart and distinguishes conflict") {
  TestDatabase database;
  auto frame = signedConfiguration(7);
  {
    runtime::SQLiteStore store(database.path());
    simulator::SimulatedTransport transport;
    simulator::SimulatedHardware hardware;
    runtime::ControllerRuntime controller(store, transport, hardware, testDevice());
    REQUIRE(controller.handleAuthenticatedFrame(frame).accepted);
    CHECK(controller.handleAuthenticatedFrame(frame).duplicate);
    CHECK(controller.handleAuthenticatedFrame(signedConfiguration(6)).error ==
          runtime::RuntimeError::replay_rejected);
  }

  runtime::SQLiteStore recovered_store(database.path());
  simulator::SimulatedTransport recovered_transport;
  simulator::SimulatedHardware recovered_hardware;
  runtime::ControllerRuntime recovered(recovered_store, recovered_transport,
                                       recovered_hardware, testDevice());
  CHECK(recovered.handleAuthenticatedFrame(frame).duplicate);
  CHECK(recovered.handleAuthenticatedFrame(signedConfiguration(7, 2)).error ==
        runtime::RuntimeError::sequence_conflict);
}

TEST_CASE("replay high-water mark advances only with the atomic domain commit") {
  TestDatabase database;
  runtime::SQLiteStore durable_store(database.path());
  simulator::FaultInjectingStore failing_store(durable_store);
  simulator::SimulatedTransport transport;
  simulator::SimulatedHardware hardware;
  auto frame = signedConfiguration(9);
  failing_store.failNextCommits(1);
  runtime::ControllerRuntime failed(failing_store, transport, hardware, testDevice());
  CHECK(failed.handleAuthenticatedFrame(frame).error ==
        runtime::RuntimeError::storage_failure);

  simulator::SimulatedHardware recovered_hardware;
  runtime::ControllerRuntime recovered(durable_store, transport, recovered_hardware,
                                       testDevice());
  CHECK(recovered.handleAuthenticatedFrame(frame).accepted);
}

TEST_CASE("SQLite triggers make the audit trail append-only") {
  TestDatabase database;
  runtime::SQLiteStore store(database.path());
  simulator::SimulatedTransport transport;
  simulator::SimulatedHardware hardware;
  runtime::ControllerRuntime controller(store, transport, hardware, testDevice());
  REQUIRE(controller.handle(core::Event{1, core::BootCompleted{}}).accepted);
  REQUIRE(controller.auditTrail(10).size() == 1);

  sqlite3 *connection = nullptr;
  REQUIRE(sqlite3_open(database.path().string().c_str(), &connection) == SQLITE_OK);
  char *error = nullptr;
  CHECK(sqlite3_exec(connection, "UPDATE audit SET detail='tampered';", nullptr, nullptr,
                     &error) == SQLITE_CONSTRAINT);
  sqlite3_free(error);
  error = nullptr;
  CHECK(sqlite3_exec(connection, "DELETE FROM audit;", nullptr, nullptr, &error) ==
        SQLITE_CONSTRAINT);
  sqlite3_free(error);
  CHECK(sqlite3_close(connection) == SQLITE_OK);
  CHECK(controller.auditTrail(10).front().detail == "boot_completed");
}

TEST_CASE("seeded sensor noise is reproducible") {
  simulator::DeterministicNoise first(0xC0FFEEU);
  simulator::DeterministicNoise second(0xC0FFEEU);
  for (std::size_t sample = 0; sample < 1'000; ++sample) {
    CHECK(first.occupancySample(true, 17) ==
          second.occupancySample(true, 17));
  }
}
