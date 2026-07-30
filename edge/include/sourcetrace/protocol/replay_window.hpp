#pragma once

#include "sourcetrace/protocol/frame.hpp"

#include <optional>

namespace sourcetrace::protocol {

enum class ReplayDecision : std::uint8_t {
  accept_new,
  duplicate,
  replay,
  conflict,
};

class ReplayWindow {
public:
  [[nodiscard]] ReplayDecision evaluate(const Frame &frame) const;
  void commit(const Frame &frame);
  void restore(std::uint64_t sequence, const Signature &signature);
  [[nodiscard]] std::optional<std::uint64_t> highWaterMark() const;

private:
  std::optional<std::uint64_t> sequence_;
  std::optional<Signature> signature_;
};

} // namespace sourcetrace::protocol
