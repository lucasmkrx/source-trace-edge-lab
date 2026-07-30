#include "sourcetrace/protocol/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
  sourcetrace::protocol::StreamParser parser;
  const auto bytes =
      std::span<const std::byte>(reinterpret_cast<const std::byte *>(data), size);
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto output = parser.feed(bytes.subspan(offset));
    if (output.consumed == 0) {
      break;
    }
    offset += output.consumed;
  }
  return 0;
}
