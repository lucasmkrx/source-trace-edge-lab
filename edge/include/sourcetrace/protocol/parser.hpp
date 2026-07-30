#pragma once

#include "sourcetrace/core/interfaces.hpp"
#include "sourcetrace/protocol/frame.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace sourcetrace::protocol {

inline constexpr std::size_t kMaxResultsPerFeed = 32;

struct ParserOutput {
  std::vector<FrameResult> results;
  std::size_t consumed{0};
  bool result_limit_reached{false};
};

class StreamParser {
public:
  explicit StreamParser(const core::ICryptoVerifier *verifier = nullptr);
  [[nodiscard]] ParserOutput feed(std::span<const std::byte> input);
  void reset();
  [[nodiscard]] std::size_t bufferedBytes() const;

private:
  [[nodiscard]] FrameResult parseCompleteFrame() const;
  void resetFrame();

  const core::ICryptoVerifier *verifier_;
  std::array<std::byte, kMaxFrameSize> buffer_{};
  std::size_t buffered_{0};
  std::size_t expected_total_{0};
};

} // namespace sourcetrace::protocol
