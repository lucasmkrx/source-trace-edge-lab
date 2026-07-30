#include "sourcetrace/core/trace.hpp"

#include "sourcetrace/core/reducer.hpp"

namespace sourcetrace::core {

ReplayResult replay(const ControllerState &initial, const std::vector<Event> &events) {
  ReplayResult result{initial, {}};
  result.trace.reserve(events.size());
  for (const auto &event : events) {
    const auto before = result.final_state;
    auto transition = reduce(before, event);
    result.trace.push_back(TraceEntry{before, event, transition.state, transition.effects});
    result.final_state = transition.state;
  }
  return result;
}

} // namespace sourcetrace::core
