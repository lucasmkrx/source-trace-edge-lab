#pragma once

#include "sourcetrace/core/types.hpp"

namespace sourcetrace::core {

[[nodiscard]] Transition reduce(const ControllerState &current, const Event &event);
[[nodiscard]] Indicator desiredIndicator(const ControllerState &state);

} // namespace sourcetrace::core
