#pragma once

#include "sourcetrace/protocol/frame.hpp"

#include <string>

namespace sourcetrace::protocol {

struct PayloadValidation {
  bool valid{false};
  std::string detail;
};

[[nodiscard]] PayloadValidation validatePayload(MessageType type, const std::string &payload);

} // namespace sourcetrace::protocol
