#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sourcetrace::protocol {

inline constexpr std::uint8_t kProtocolVersion = 1;
inline constexpr std::size_t kLengthPrefixSize = 4;
inline constexpr std::size_t kHeaderSize = 44;
inline constexpr std::size_t kCrcSize = 4;
inline constexpr std::size_t kSignatureSize = 64;
inline constexpr std::size_t kMaxPayloadSize = 4096;
inline constexpr std::size_t kMinBodySize = kHeaderSize + kCrcSize;
inline constexpr std::size_t kMaxBodySize =
    kHeaderSize + kMaxPayloadSize + kSignatureSize + kCrcSize;
inline constexpr std::size_t kMaxFrameSize = kLengthPrefixSize + kMaxBodySize;
inline constexpr std::uint8_t kFlagAuthenticated = 0x01;
inline constexpr std::uint8_t kFlagError = 0x02;
inline constexpr std::uint8_t kAllowedFlags = kFlagAuthenticated | kFlagError;

using DeviceId = std::array<std::byte, 16>;
using Signature = std::array<std::byte, kSignatureSize>;

enum class MessageType : std::uint8_t {
  registration = 1,
  configuration = 2,
  command = 3,
  telemetry = 4,
  acknowledgement = 5,
  error = 6,
};

// The two-byte base type is the normative wire width and leaves versioned space.
enum class ErrorCode : std::uint16_t { // NOLINT(performance-enum-size)
  none = 0,
  invalid_length = 1,
  invalid_magic = 2,
  unsupported_version = 3,
  invalid_header = 4,
  invalid_message_type = 5,
  reserved_flags = 6,
  invalid_error_code = 7,
  payload_length_mismatch = 8,
  crc_mismatch = 9,
  authentication_required = 10,
  signature_invalid = 11,
  json_invalid = 12,
  schema_invalid = 13,
  replay_rejected = 14,
  sequence_conflict = 15,
  internal_error = 16,
};

struct Frame {
  MessageType type{MessageType::telemetry};
  std::uint8_t flags{0};
  ErrorCode error{ErrorCode::none};
  DeviceId device_id{};
  std::uint64_t sequence{0};
  std::uint64_t monotonic_ms{0};
  std::string payload{"{}"};
  std::optional<Signature> signature;

  bool operator==(const Frame &) const = default;
};

struct FrameResult {
  std::optional<Frame> frame;
  ErrorCode error{ErrorCode::none};
  std::string detail;

  [[nodiscard]] bool ok() const { return frame.has_value(); }
};

struct EncodeResult {
  std::vector<std::byte> bytes;
  ErrorCode error{ErrorCode::none};
  std::string detail;

  [[nodiscard]] bool ok() const { return error == ErrorCode::none; }
};

[[nodiscard]] bool requiresAuthentication(MessageType type);
[[nodiscard]] EncodeResult encode(const Frame &frame);
[[nodiscard]] std::vector<std::byte> authenticationMessage(const Frame &frame);
[[nodiscard]] std::string toHex(std::span<const std::byte> bytes);

} // namespace sourcetrace::protocol
