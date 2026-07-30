#pragma once

#include "sourcetrace/core/types.hpp"

#include <vector>

namespace sourcetrace::core {

struct TraceEntry {
  ControllerState before;
  Event event;
  ControllerState after;
  std::vector<Effect> effects;

  bool operator==(const TraceEntry &) const = default;
};

struct ReplayResult {
  ControllerState final_state;
  std::vector<TraceEntry> trace;

  bool operator==(const ReplayResult &) const = default;
};

[[nodiscard]] ReplayResult replay(const ControllerState &initial,
                                  const std::vector<Event> &events);

} // namespace sourcetrace::core
