#include "sourcetrace/simulator/deterministic_simulator.hpp"

#include <algorithm>
#include <stdexcept>

namespace sourcetrace::simulator {

std::uint64_t VirtualClock::monotonicMs() const { return now_ms_; }

void VirtualClock::advanceTo(std::uint64_t at_ms) {
  if (at_ms < now_ms_) {
    throw std::invalid_argument("virtual clock cannot move backward");
  }
  now_ms_ = at_ms;
}

void SimulatedHardware::schedule(core::Event event) {
  const auto position = std::upper_bound(
      events_.begin(), events_.end(), event.at_ms,
      [](const std::uint64_t at_ms, const core::Event &candidate) {
        return at_ms < candidate.at_ms;
      });
  events_.insert(position, event);
}

std::optional<core::Event> SimulatedHardware::poll() {
  if (events_.empty()) {
    return std::nullopt;
  }
  auto event = events_.front();
  events_.pop_front();
  return event;
}

void SimulatedHardware::setIndicator(core::Indicator indicator) { indicator_ = indicator; }

core::Indicator SimulatedHardware::indicator() const { return indicator_; }

DeterministicSimulator::DeterministicSimulator(core::ControllerState initial)
    : initial_(initial) {}

core::ReplayResult DeterministicSimulator::run(const std::vector<core::Event> &events) {
  return core::replay(initial_, events);
}

} // namespace sourcetrace::simulator
