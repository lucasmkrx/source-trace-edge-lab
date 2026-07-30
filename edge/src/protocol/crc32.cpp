#include "sourcetrace/protocol/crc32.hpp"

namespace sourcetrace::protocol {

std::uint32_t crc32(std::span<const std::byte> bytes) {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (const auto value : bytes) {
    crc ^= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(value));
    for (int bit = 0; bit < 8; ++bit) {
      const auto mask = static_cast<std::uint32_t>(-(static_cast<std::int32_t>(crc & 1U)));
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

} // namespace sourcetrace::protocol
