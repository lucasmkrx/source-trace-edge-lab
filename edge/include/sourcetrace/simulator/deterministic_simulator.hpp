#pragma once

#include "sourcetrace/core/interfaces.hpp"
#include "sourcetrace/core/trace.hpp"

#include <cstdint>
#include <deque>
#include <vector>

namespace sourcetrace::simulator {

class VirtualClock final : public core::IClock {
public:
  [[nodiscard]] std::uint64_t monotonicMs() const override;
  void advanceTo(std::uint64_t at_ms);

private:
  std::uint64_t now_ms_{0};
};

class SimulatedHardware final : public core::IInputs, public core::IOutputs {
public:
  void schedule(core::Event event);
  [[nodiscard]] std::optional<core::Event> poll() override;
  void setIndicator(core::Indicator indicator) override;
  [[nodiscard]] core::Indicator indicator() const;

private:
  std::deque<core::Event> events_;
  core::Indicator indicator_{core::Indicator::booting};
};

class DeterministicSimulator {
public:
  explicit DeterministicSimulator(core::ControllerState initial = {});
  [[nodiscard]] core::ReplayResult run(const std::vector<core::Event> &events);

private:
  core::ControllerState initial_;
};

} // namespace sourcetrace::simulator
