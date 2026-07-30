#include "sourcetrace/protocol/frame.hpp"

#include "sourcetrace/protocol/crc32.hpp"
#include "sourcetrace/protocol/payload_validation.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>

namespace sourcetrace::protocol {
namespace {

constexpr std::array<std::byte, 2> kMagic{std::byte{'S'}, std::byte{'T'}};

void appendU16(std::vector<std::byte> &output, std::uint16_t value) {
  output.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
  output.push_back(static_cast<std::byte>(value & 0xFFU));
}

void appendU32(std::vector<std::byte> &output, std::uint32_t value) {
  for (int shift = 24; shift >= 0; shift -= 8) {
    output.push_back(static_cast<std::byte>((value >> static_cast<unsigned>(shift)) & 0xFFU));
  }
}

void appendU64(std::vector<std::byte> &output, std::uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    output.push_back(static_cast<std::byte>((value >> static_cast<unsigned>(shift)) & 0xFFU));
  }
}

std::vector<std::byte> headerAndPayload(const Frame &frame) {
  std::vector<std::byte> output;
  output.reserve(kHeaderSize + frame.payload.size());
  output.insert(output.end(), kMagic.begin(), kMagic.end());
  output.push_back(static_cast<std::byte>(kProtocolVersion));
  output.push_back(static_cast<std::byte>(kHeaderSize));
  output.push_back(static_cast<std::byte>(frame.type));
  output.push_back(static_cast<std::byte>(frame.flags));
  appendU16(output, static_cast<std::uint16_t>(frame.error));
  output.insert(output.end(), frame.device_id.begin(), frame.device_id.end());
  appendU64(output, frame.sequence);
  appendU64(output, frame.monotonic_ms);
  appendU32(output, static_cast<std::uint32_t>(frame.payload.size()));
  std::transform(frame.payload.begin(), frame.payload.end(), std::back_inserter(output),
                 [](char value) { return static_cast<std::byte>(value); });
  return output;
}

EncodeResult invalid(ErrorCode error, std::string detail) {
  return EncodeResult{{}, error, std::move(detail)};
}

} // namespace

bool requiresAuthentication(MessageType type) {
  return type == MessageType::configuration || type == MessageType::command;
}

std::vector<std::byte> authenticationMessage(const Frame &frame) {
  static constexpr std::array<std::byte, 19> domain{
      std::byte{'S'}, std::byte{'o'}, std::byte{'u'}, std::byte{'r'}, std::byte{'c'},
      std::byte{'e'}, std::byte{'T'}, std::byte{'r'}, std::byte{'a'}, std::byte{'c'},
      std::byte{'e'}, std::byte{'E'}, std::byte{'d'}, std::byte{'g'}, std::byte{'e'},
      std::byte{'/'}, std::byte{'v'}, std::byte{'1'}, std::byte{0}};
  auto encoded = headerAndPayload(frame);
  std::vector<std::byte> message;
  message.reserve(domain.size() + encoded.size());
  message.insert(message.end(), domain.begin(), domain.end());
  message.insert(message.end(), encoded.begin(), encoded.end());
  return message;
}

EncodeResult encode(const Frame &frame) {
  const auto type_value = static_cast<std::uint8_t>(frame.type);
  if (type_value < static_cast<std::uint8_t>(MessageType::registration) ||
      type_value > static_cast<std::uint8_t>(MessageType::error)) {
    return invalid(ErrorCode::invalid_message_type, "message type is outside the v1 enum");
  }
  if (static_cast<std::uint16_t>(frame.error) >
      static_cast<std::uint16_t>(ErrorCode::internal_error)) {
    return invalid(ErrorCode::invalid_error_code, "error code is outside the v1 enum");
  }
  if (frame.payload.size() > kMaxPayloadSize) {
    return invalid(ErrorCode::invalid_length, "payload exceeds 4096 bytes");
  }
  if ((frame.flags & static_cast<std::uint8_t>(~kAllowedFlags)) != 0U) {
    return invalid(ErrorCode::reserved_flags, "reserved flag is set");
  }
  const bool authenticated = (frame.flags & kFlagAuthenticated) != 0U;
  if (requiresAuthentication(frame.type) && !authenticated) {
    return invalid(ErrorCode::authentication_required,
                   "configuration and command frames require authentication");
  }
  if (authenticated != frame.signature.has_value()) {
    return invalid(ErrorCode::invalid_header, "signature presence does not match flags");
  }
  const bool error_flag = (frame.flags & kFlagError) != 0U;
  if ((frame.type == MessageType::error) != error_flag ||
      (error_flag && frame.error == ErrorCode::none) ||
      (!error_flag && frame.error != ErrorCode::none)) {
    return invalid(ErrorCode::invalid_error_code, "error type, flag, and code disagree");
  }
  const auto payload_validation = validatePayload(frame.type, frame.payload);
  if (!payload_validation.valid) {
    return invalid(ErrorCode::schema_invalid, payload_validation.detail);
  }

  auto body = headerAndPayload(frame);
  if (frame.signature) {
    body.insert(body.end(), frame.signature->begin(), frame.signature->end());
  }
  appendU32(body, crc32(body));
  if (body.size() > kMaxBodySize) {
    return invalid(ErrorCode::invalid_length, "encoded body exceeds protocol maximum");
  }

  std::vector<std::byte> output;
  output.reserve(kLengthPrefixSize + body.size());
  appendU32(output, static_cast<std::uint32_t>(body.size()));
  output.insert(output.end(), body.begin(), body.end());
  return EncodeResult{std::move(output), ErrorCode::none, {}};
}

std::string toHex(std::span<const std::byte> bytes) {
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const auto value : bytes) {
    output << std::setw(2) << static_cast<unsigned>(std::to_integer<std::uint8_t>(value));
  }
  return output.str();
}

} // namespace sourcetrace::protocol
