#include "sourcetrace/core/reducer.hpp"
#include "sourcetrace/core/trace.hpp"
#include "sourcetrace/simulator/deterministic_simulator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

using namespace sourcetrace::core;

ControllerState configuredState() {
  auto state = reduce({}, Event{0, BootCompleted{}}).state;
  state = reduce(state, Event{1, ConfigurationAccepted{ControllerConfig{7, 1'000, true}}})
              .state;
  return state;
}

template <typename T> bool hasEffect(const Transition &transition) {
  return std::any_of(transition.effects.begin(), transition.effects.end(),
                     [](const Effect &effect) { return std::holds_alternative<T>(effect.data); });
}

} // namespace

TEST_CASE("boot remains degraded until configuration and connectivity are established") {
  auto booted = reduce({}, Event{0, BootCompleted{}});
  CHECK(booted.state.mode == ControllerMode::idle);
  CHECK(booted.state.indicator == Indicator::degraded);
  CHECK(hasEffect<PersistCheckpoint>(booted));

  auto configured =
      reduce(booted.state, Event{1, ConfigurationAccepted{ControllerConfig{1, 5'000, true}}});
  CHECK(configured.state.configuration_valid);
  CHECK(configured.state.indicator == Indicator::degraded);

  auto connected = reduce(configured.state, Event{2, ConnectivityChanged{true}});
  CHECK(connected.state.health == Health::nominal);
  CHECK(connected.state.indicator == Indicator::ready);
  CHECK(hasEffect<AttemptFlush>(connected));
}

TEST_CASE("physical start fails closed until occupancy and configuration allow it") {
  auto unconfigured = reduce(reduce({}, Event{0, BootCompleted{}}).state,
                             Event{1, SessionButtonPressed{}});
  REQUIRE(hasEffect<RejectInput>(unconfigured));
  CHECK(std::get<RejectInput>(unconfigured.effects.front().data).reason ==
        RejectionReason::configuration_required);

  auto state = configuredState();
  auto unoccupied = reduce(state, Event{2, SessionButtonPressed{}});
  CHECK(unoccupied.state.mode == ControllerMode::idle);
  CHECK(hasEffect<RejectInput>(unoccupied));

  state = reduce(state, Event{2, OccupancyChanged{true}}).state;
  auto started = reduce(state, Event{3, SessionButtonPressed{}});
  CHECK(started.state.mode == ControllerMode::session_active);
  CHECK(hasEffect<QueueTelemetry>(started));
}

TEST_CASE("offline session remains locally active and requests a bounded recovery flush") {
  auto state = configuredState();
  state = reduce(state, Event{2, ConnectivityChanged{true}}).state;
  state = reduce(state, Event{3, OccupancyChanged{true}}).state;
  state = reduce(state, Event{4, SessionButtonPressed{}}).state;

  auto lost = reduce(state, Event{5, ConnectivityChanged{false}});
  CHECK(lost.state.mode == ControllerMode::session_active);
  CHECK(lost.state.health == Health::network_degraded);
  CHECK(lost.state.indicator == Indicator::degraded);
  CHECK_FALSE(hasEffect<AttemptFlush>(lost));

  auto restored = reduce(lost.state, Event{6, ConnectivityChanged{true}});
  CHECK(restored.state.mode == ControllerMode::session_active);
  CHECK(restored.state.indicator == Indicator::active);
  CHECK(hasEffect<AttemptFlush>(restored));
}

TEST_CASE("storage failure blocks a new session but never blocks a physical stop") {
  auto state = configuredState();
  state = reduce(state, Event{2, OccupancyChanged{true}}).state;
  auto failed = reduce(state, Event{3, StorageFailed{}});
  CHECK(failed.state.health == Health::storage_fault);
  CHECK(failed.state.indicator == Indicator::fault);

  auto rejected = reduce(failed.state, Event{4, SessionButtonPressed{}});
  CHECK(rejected.state.mode == ControllerMode::idle);
  CHECK(hasEffect<RejectInput>(rejected));

  state = reduce(state, Event{3, SessionButtonPressed{}}).state;
  state = reduce(state, Event{4, StorageFailed{}}).state;
  auto stopped = reduce(state, Event{5, SessionButtonPressed{}});
  CHECK(stopped.state.mode == ControllerMode::idle);
  CHECK(hasEffect<WriteAudit>(stopped));
  CHECK_FALSE(hasEffect<PersistCheckpoint>(stopped));
}

TEST_CASE("time regression has no state effect and emits a volatile rejection") {
  auto state = configuredState();
  state.monotonic_ms = 100;
  auto transition = reduce(state, Event{99, Tick{}});
  CHECK(transition.state == state);
  REQUIRE(hasEffect<RejectInput>(transition));
  CHECK_FALSE(transition.effects.front().durable);
}

TEST_CASE("periodic telemetry uses only explicit monotonic event time") {
  auto state = configuredState();
  auto early = reduce(state, Event{999, Tick{}});
  CHECK_FALSE(hasEffect<QueueTelemetry>(early));
  auto due = reduce(early.state, Event{1'000, Tick{}});
  CHECK(hasEffect<QueueTelemetry>(due));
  CHECK(due.state.last_telemetry_ms == 1'000);
}

TEST_CASE("replaying the same trace is byte-for-behavior deterministic") {
  const std::vector<Event> events{
      {0, BootCompleted{}},
      {1, ConfigurationAccepted{ControllerConfig{1, 1'000, true}}},
      {2, ConnectivityChanged{true}},
      {3, OccupancyChanged{true}},
      {4, SessionButtonPressed{}},
      {1'004, Tick{}},
      {1'005, ConnectivityChanged{false}},
      {2'005, Tick{}},
  };
  const auto first = replay({}, events);
  const auto second = replay({}, events);
  CHECK(first == second);
  CHECK(first.trace.size() == events.size());
  CHECK(first.final_state.mode == ControllerMode::session_active);
}

TEST_CASE("simulated hardware preserves deterministic ordering for equal timestamps") {
  sourcetrace::simulator::SimulatedHardware hardware;
  hardware.schedule(Event{2, ConnectivityChanged{true}});
  hardware.schedule(Event{1, OccupancyChanged{true}});
  hardware.schedule(Event{2, SessionButtonPressed{}});

  REQUIRE(hardware.poll()->at_ms == 1);
  auto second = hardware.poll();
  REQUIRE(second.has_value());
  CHECK(std::holds_alternative<ConnectivityChanged>(second->data));
  auto third = hardware.poll();
  REQUIRE(third.has_value());
  CHECK(std::holds_alternative<SessionButtonPressed>(third->data));
  CHECK_FALSE(hardware.poll().has_value());
}
