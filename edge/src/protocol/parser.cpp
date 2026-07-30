#include "sourcetrace/protocol/parser.hpp"

#include "sourcetrace/protocol/crc32.hpp"
#include "sourcetrace/protocol/payload_validation.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace sourcetrace::protocol {
namespace {

std::uint16_t readU16(std::span<const std::byte> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) << 8U) |
      static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])));
}

std::uint32_t readU32(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < 4; ++index) {
    value = (value << 8U) | std::to_integer<std::uint8_t>(bytes[offset + index]);
  }
  return value;
}

std::uint64_t readU64(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < 8; ++index) {
    value = (value << 8U) | std::to_integer<std::uint8_t>(bytes[offset + index]);
  }
  return value;
}

bool validMessageType(std::uint8_t value) {
  return value >= static_cast<std::uint8_t>(MessageType::registration) &&
         value <= static_cast<std::uint8_t>(MessageType::error);
}

bool validErrorCode(std::uint16_t value) {
  return value <= static_cast<std::uint16_t>(ErrorCode::internal_error);
}

FrameResult failure(ErrorCode error, std::string detail) {
  return FrameResult{std::nullopt, error, std::move(detail)};
}

} // namespace

StreamParser::StreamParser(const core::ICryptoVerifier *verifier) : verifier_(verifier) {}

ParserOutput StreamParser::feed(std::span<const std::byte> input) {
  ParserOutput output;
  output.results.reserve(std::min<std::size_t>(kMaxResultsPerFeed, input.size() / kMinBodySize + 1));

  while (output.consumed < input.size() && output.results.size() < kMaxResultsPerFeed) {
    if (expected_total_ == 0 && buffered_ < kLengthPrefixSize) {
      const auto needed = kLengthPrefixSize - buffered_;
      const auto available = input.size() - output.consumed;
      const auto count = std::min(needed, available);
      std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(output.consumed), count,
                  buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_));
      buffered_ += count;
      output.consumed += count;
      if (buffered_ < kLengthPrefixSize) {
        break;
      }

      const auto body_length = static_cast<std::size_t>(
          readU32(std::span<const std::byte>(buffer_.data(), kLengthPrefixSize), 0));
      if (body_length < kMinBodySize || body_length > kMaxBodySize) {
        output.results.push_back(
            failure(ErrorCode::invalid_length, "outer frame length is outside v1 bounds"));
        resetFrame();
        continue;
      }
      expected_total_ = kLengthPrefixSize + body_length;
    }

    const auto needed = expected_total_ - buffered_;
    const auto available = input.size() - output.consumed;
    const auto count = std::min(needed, available);
    std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(output.consumed), count,
                buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_));
    buffered_ += count;
    output.consumed += count;
    if (buffered_ == expected_total_) {
      output.results.push_back(parseCompleteFrame());
      resetFrame();
    }
  }

  output.result_limit_reached =
      output.results.size() == kMaxResultsPerFeed && output.consumed < input.size();
  return output;
}

void StreamParser::reset() { resetFrame(); }

std::size_t StreamParser::bufferedBytes() const { return buffered_; }

void StreamParser::resetFrame() {
  buffered_ = 0;
  expected_total_ = 0;
}

FrameResult StreamParser::parseCompleteFrame() const {
  const std::span<const std::byte> frame_bytes(buffer_.data(), expected_total_);
  const auto body = frame_bytes.subspan(kLengthPrefixSize);
  if (body[0] != std::byte{'S'} || body[1] != std::byte{'T'}) {
    return failure(ErrorCode::invalid_magic, "magic is not ST");
  }
  if (std::to_integer<std::uint8_t>(body[2]) != kProtocolVersion) {
    return failure(ErrorCode::unsupported_version, "protocol version is unsupported");
  }
  if (std::to_integer<std::uint8_t>(body[3]) != kHeaderSize) {
    return failure(ErrorCode::invalid_header, "v1 header length must be 44");
  }
  const auto type_value = std::to_integer<std::uint8_t>(body[4]);
  if (!validMessageType(type_value)) {
    return failure(ErrorCode::invalid_message_type, "message type is outside the v1 enum");
  }
  const auto type = static_cast<MessageType>(type_value);
  const auto flags = std::to_integer<std::uint8_t>(body[5]);
  if ((flags & static_cast<std::uint8_t>(~kAllowedFlags)) != 0U) {
    return failure(ErrorCode::reserved_flags, "reserved flag is set");
  }
  const bool authenticated = (flags & kFlagAuthenticated) != 0U;
  const bool error_flag = (flags & kFlagError) != 0U;
  const auto error_value = readU16(body, 6);
  if (!validErrorCode(error_value)) {
    return failure(ErrorCode::invalid_error_code, "error code is outside the v1 enum");
  }
  const auto error = static_cast<ErrorCode>(error_value);
  if ((type == MessageType::error) != error_flag ||
      (error_flag && error == ErrorCode::none) ||
      (!error_flag && error != ErrorCode::none)) {
    return failure(ErrorCode::invalid_error_code, "error type, flag, and code disagree");
  }
  if (requiresAuthentication(type) && !authenticated) {
    return failure(ErrorCode::authentication_required,
                   "configuration and command frames must be authenticated");
  }

  const auto payload_size = static_cast<std::size_t>(readU32(body, 40));
  if (payload_size > kMaxPayloadSize) {
    return failure(ErrorCode::invalid_length, "payload length exceeds 4096 bytes");
  }
  const auto signature_size = authenticated ? kSignatureSize : 0U;
  if (payload_size > std::numeric_limits<std::size_t>::max() - kHeaderSize - signature_size -
                         kCrcSize) {
    return failure(ErrorCode::invalid_length, "payload length overflows frame size");
  }
  const auto calculated_body_size = kHeaderSize + payload_size + signature_size + kCrcSize;
  if (calculated_body_size != body.size()) {
    return failure(ErrorCode::payload_length_mismatch,
                   "payload length does not match the outer frame");
  }

  const auto supplied_crc = readU32(body, body.size() - kCrcSize);
  const auto protected_bytes = body.first(body.size() - kCrcSize);
  if (crc32(protected_bytes) != supplied_crc) {
    return failure(ErrorCode::crc_mismatch, "CRC32 corruption check failed");
  }

  Frame frame;
  frame.type = type;
  frame.flags = flags;
  frame.error = error;
  std::copy_n(body.begin() + 8, frame.device_id.size(), frame.device_id.begin());
  frame.sequence = readU64(body, 24);
  frame.monotonic_ms = readU64(body, 32);
  frame.payload.clear();
  frame.payload.reserve(payload_size);
  for (const auto value : body.subspan(kHeaderSize, payload_size)) {
    frame.payload.push_back(static_cast<char>(std::to_integer<unsigned char>(value)));
  }

  if (authenticated) {
    Signature signature{};
    std::copy_n(body.begin() + static_cast<std::ptrdiff_t>(kHeaderSize + payload_size),
                signature.size(), signature.begin());
    frame.signature = signature;
    if (verifier_ == nullptr) {
      return failure(ErrorCode::signature_invalid, "no signature verifier is configured");
    }
    const auto message = authenticationMessage(frame);
    if (!verifier_->verify(message, *frame.signature)) {
      return failure(ErrorCode::signature_invalid, "Ed25519 signature verification failed");
    }
  }

  const auto validation = validatePayload(type, frame.payload);
  if (!validation.valid) {
    const auto error_code = validation.detail == "payload is not valid JSON"
                                ? ErrorCode::json_invalid
                                : ErrorCode::schema_invalid;
    return failure(error_code, validation.detail);
  }
  return FrameResult{std::move(frame), ErrorCode::none, {}};
}

} // namespace sourcetrace::protocol
