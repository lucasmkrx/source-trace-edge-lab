#include "sourcetrace/crypto/ed25519.hpp"
#include "sourcetrace/protocol/crc32.hpp"
#include "sourcetrace/protocol/frame.hpp"
#include "sourcetrace/protocol/parser.hpp"
#include "sourcetrace/protocol/replay_window.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace sourcetrace;

crypto::KeyPair testKeys() {
  crypto::Seed seed{};
  for (std::size_t index = 0; index < seed.size(); ++index) {
    seed[index] = static_cast<std::byte>(index);
  }
  return crypto::keyPairFromSeed(seed);
}

protocol::DeviceId testDevice() {
  protocol::DeviceId id{};
  for (std::size_t index = 0; index < id.size(); ++index) {
    id[index] = static_cast<std::byte>(index);
  }
  return id;
}

protocol::Frame telemetryFrame(std::string event_id = "evt-1") {
  return protocol::Frame{
      protocol::MessageType::telemetry,
      0,
      protocol::ErrorCode::none,
      testDevice(),
      1,
      2,
      "{\"eventId\":\"" + event_id +
          "\",\"health\":\"nominal\",\"mode\":\"idle\",\"occupied\":false,\"queueDepth\":0}",
      std::nullopt,
  };
}

protocol::Frame signedConfiguration(std::uint64_t sequence = 7) {
  auto frame = protocol::Frame{
      protocol::MessageType::configuration,
      protocol::kFlagAuthenticated,
      protocol::ErrorCode::none,
      testDevice(),
      sequence,
      50,
      "{\"configVersion\":3,\"occupancyRequiredToStart\":true,"
      "\"telemetryIntervalMs\":1000}",
      std::nullopt,
  };
  const auto keys = testKeys();
  frame.signature = crypto::signDetached(protocol::authenticationMessage(frame), keys.secret_key);
  return frame;
}

void writeU32(std::vector<std::byte> &bytes, std::size_t offset, std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index) {
    const auto shift = static_cast<unsigned>(24U - index * 8U);
    bytes[offset + index] = static_cast<std::byte>((value >> shift) & 0xFFU);
  }
}

void refreshCrc(std::vector<std::byte> &bytes) {
  const auto crc_offset = bytes.size() - protocol::kCrcSize;
  const auto body = std::span<const std::byte>(bytes).subspan(
      protocol::kLengthPrefixSize, crc_offset - protocol::kLengthPrefixSize);
  writeU32(bytes, crc_offset, protocol::crc32(body));
}

protocol::FrameResult parseOne(std::span<const std::byte> bytes,
                               const core::ICryptoVerifier *verifier = nullptr) {
  protocol::StreamParser parser(verifier);
  auto output = parser.feed(bytes);
  REQUIRE(output.consumed == bytes.size());
  REQUIRE(output.results.size() == 1);
  return output.results.front();
}

} // namespace

TEST_CASE("CRC32 matches the standard check vector and is not used as authentication") {
  const std::string check = "123456789";
  const auto bytes =
      std::span<const std::byte>(reinterpret_cast<const std::byte *>(check.data()), check.size());
  CHECK(protocol::crc32(bytes) == 0xCBF43926U);
}

TEST_CASE("unsigned telemetry round trips through arbitrarily partial input") {
  const auto frame = telemetryFrame();
  const auto encoded = protocol::encode(frame);
  REQUIRE(encoded.ok());
  INFO(protocol::toHex(encoded.bytes));
  protocol::StreamParser parser;
  std::vector<protocol::FrameResult> results;
  for (const auto value : encoded.bytes) {
    const std::array one{value};
    auto output = parser.feed(one);
    results.insert(results.end(), output.results.begin(), output.results.end());
  }
  REQUIRE(results.size() == 1);
  INFO(results.front().detail);
  REQUIRE(results.front().ok());
  CHECK(*results.front().frame == frame);
}

TEST_CASE("encoder matches the normative telemetry golden vector") {
  std::ifstream input(std::string(SOURCETRACE_SOURCE_DIR) +
                      "/docs/protocol/golden-vectors.json");
  REQUIRE(input.good());
  const auto vectors = nlohmann::json::parse(input);
  const auto encoded = protocol::encode(telemetryFrame());
  REQUIRE(encoded.ok());
  CHECK(protocol::toHex(encoded.bytes) ==
        vectors.at("unsignedTelemetry").at("frameHex").get<std::string>());
  CHECK(encoded.bytes.size() ==
        vectors.at("unsignedTelemetry").at("totalBytes").get<std::size_t>());
}

TEST_CASE("signed configuration verifies before its bounded JSON is accepted") {
  const auto keys = testKeys();
  crypto::Ed25519Verifier verifier(keys.public_key);
  const auto frame = signedConfiguration();
  const auto encoded = protocol::encode(frame);
  REQUIRE(encoded.ok());
  const auto parsed = parseOne(encoded.bytes, &verifier);
  INFO(parsed.detail);
  REQUIRE(parsed.ok());
  CHECK(parsed.frame == frame);
}

TEST_CASE("configuration and command encoding fail without a detached signature") {
  auto frame = signedConfiguration();
  frame.flags = 0;
  frame.signature.reset();
  const auto encoded = protocol::encode(frame);
  CHECK_FALSE(encoded.ok());
  CHECK(encoded.error == protocol::ErrorCode::authentication_required);
}

TEST_CASE("CRC corruption is rejected before payload interpretation") {
  auto encoded = protocol::encode(telemetryFrame());
  REQUIRE(encoded.ok());
  encoded.bytes[protocol::kLengthPrefixSize + protocol::kHeaderSize + 2] ^= std::byte{0x01};
  const auto parsed = parseOne(encoded.bytes);
  CHECK_FALSE(parsed.ok());
  CHECK(parsed.error == protocol::ErrorCode::crc_mismatch);
}

TEST_CASE("signature corruption with a valid CRC is rejected cryptographically") {
  const auto keys = testKeys();
  crypto::Ed25519Verifier verifier(keys.public_key);
  auto encoded = protocol::encode(signedConfiguration());
  REQUIRE(encoded.ok());
  encoded.bytes[encoded.bytes.size() - protocol::kCrcSize - 1] ^= std::byte{0x80};
  refreshCrc(encoded.bytes);
  const auto parsed = parseOne(encoded.bytes, &verifier);
  CHECK_FALSE(parsed.ok());
  CHECK(parsed.error == protocol::ErrorCode::signature_invalid);
}

TEST_CASE("truncated and partial frames remain bounded and await completion") {
  const auto encoded = protocol::encode(telemetryFrame());
  REQUIRE(encoded.ok());
  for (std::size_t length = 0; length < encoded.bytes.size(); ++length) {
    protocol::StreamParser parser;
    const auto output = parser.feed(
        std::span<const std::byte>(encoded.bytes).first(length));
    CHECK(output.results.empty());
    CHECK(parser.bufferedBytes() <= protocol::kMaxFrameSize);
  }
}

TEST_CASE("oversized outer length is rejected after four bytes without body allocation") {
  const std::array<std::byte, 4> oversized{
      std::byte{0x00}, std::byte{0x00}, std::byte{0x20}, std::byte{0x00}};
  protocol::StreamParser parser;
  const auto output = parser.feed(oversized);
  REQUIRE(output.results.size() == 1);
  CHECK(output.results.front().error == protocol::ErrorCode::invalid_length);
  CHECK(parser.bufferedBytes() == 0);
}

TEST_CASE("inner payload overflow and mismatch cannot wrap calculated frame size") {
  auto encoded = protocol::encode(telemetryFrame());
  REQUIRE(encoded.ok());
  writeU32(encoded.bytes, protocol::kLengthPrefixSize + 40, 0xFFFFFFFFU);
  refreshCrc(encoded.bytes);
  const auto parsed = parseOne(encoded.bytes);
  CHECK(parsed.error == protocol::ErrorCode::invalid_length);
}

TEST_CASE("header version enum flags and error code are validated explicitly") {
  const auto valid = protocol::encode(telemetryFrame());
  REQUIRE(valid.ok());

  const std::array<std::pair<std::size_t, protocol::ErrorCode>, 4> cases{{
      {6, protocol::ErrorCode::unsupported_version},
      {7, protocol::ErrorCode::invalid_header},
      {8, protocol::ErrorCode::invalid_message_type},
      {9, protocol::ErrorCode::reserved_flags},
  }};
  for (const auto &[offset, expected] : cases) {
    auto bytes = valid.bytes;
    bytes[offset] = std::byte{0xFF};
    refreshCrc(bytes);
    CHECK(parseOne(bytes).error == expected);
  }

  auto error_enum = valid.bytes;
  error_enum[10] = std::byte{0xFF};
  error_enum[11] = std::byte{0xFF};
  refreshCrc(error_enum);
  CHECK(parseOne(error_enum).error == protocol::ErrorCode::invalid_error_code);
}

TEST_CASE("invalid JSON and valid JSON with the wrong schema have distinct errors") {
  auto invalid_json = telemetryFrame();
  invalid_json.payload = "{";
  // Encoder correctly refuses invalid JSON, so mutate a valid wire frame instead.
  auto encoded = protocol::encode(telemetryFrame());
  REQUIRE(encoded.ok());
  const auto payload_offset = protocol::kLengthPrefixSize + protocol::kHeaderSize;
  encoded.bytes[payload_offset] = std::byte{'['};
  refreshCrc(encoded.bytes);
  CHECK(parseOne(encoded.bytes).error == protocol::ErrorCode::json_invalid);

  auto wrong_schema = telemetryFrame();
  wrong_schema.payload = "{}";
  const auto wrong_encoded = protocol::encode(wrong_schema);
  CHECK_FALSE(wrong_encoded.ok());
  CHECK(wrong_encoded.error == protocol::ErrorCode::schema_invalid);
}

TEST_CASE("replay window distinguishes new duplicate replay and sequence conflict") {
  protocol::ReplayWindow window;
  auto first = signedConfiguration(7);
  CHECK(window.evaluate(first) == protocol::ReplayDecision::accept_new);
  window.commit(first);
  CHECK(window.evaluate(first) == protocol::ReplayDecision::duplicate);

  auto older = signedConfiguration(6);
  CHECK(window.evaluate(older) == protocol::ReplayDecision::replay);

  auto conflict = first;
  conflict.payload =
      "{\"configVersion\":4,\"occupancyRequiredToStart\":true,"
      "\"telemetryIntervalMs\":1000}";
  const auto keys = testKeys();
  conflict.signature =
      crypto::signDetached(protocol::authenticationMessage(conflict), keys.secret_key);
  CHECK(window.evaluate(conflict) == protocol::ReplayDecision::conflict);

  auto newer = signedConfiguration(8);
  CHECK(window.evaluate(newer) == protocol::ReplayDecision::accept_new);
  CHECK(window.highWaterMark() == 7);
}

TEST_CASE("property-style chunking and payload generation preserves round trips") {
  std::mt19937 generator(0x5354U);
  std::uniform_int_distribution<int> character('a', 'z');
  for (std::size_t sample = 0; sample < 200; ++sample) {
    std::string event_id(1 + sample % 64, 'a');
    std::generate(event_id.begin(), event_id.end(),
                  [&] { return static_cast<char>(character(generator)); });
    const auto frame = telemetryFrame(event_id);
    const auto encoded = protocol::encode(frame);
    REQUIRE(encoded.ok());

    protocol::StreamParser parser;
    std::vector<protocol::FrameResult> results;
    std::size_t offset = 0;
    const std::size_t chunk_size = 1 + sample % 17;
    while (offset < encoded.bytes.size()) {
      const auto count = std::min(chunk_size, encoded.bytes.size() - offset);
      auto output = parser.feed(
          std::span<const std::byte>(encoded.bytes).subspan(offset, count));
      offset += output.consumed;
      results.insert(results.end(), output.results.begin(), output.results.end());
    }
    REQUIRE(results.size() == 1);
    INFO("sample=" << sample << " detail=" << results.front().detail);
    REQUIRE(results.front().ok());
    CHECK(results.front().frame == frame);
  }
}

TEST_CASE("one feed caps result growth and reports unconsumed input") {
  const auto encoded = protocol::encode(telemetryFrame());
  REQUIRE(encoded.ok());
  std::vector<std::byte> many;
  for (std::size_t index = 0; index < protocol::kMaxResultsPerFeed + 5; ++index) {
    many.insert(many.end(), encoded.bytes.begin(), encoded.bytes.end());
  }
  protocol::StreamParser parser;
  const auto output = parser.feed(many);
  CHECK(output.results.size() == protocol::kMaxResultsPerFeed);
  CHECK(output.result_limit_reached);
  CHECK(output.consumed < many.size());
}
