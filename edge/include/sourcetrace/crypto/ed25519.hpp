#pragma once

#include "sourcetrace/core/interfaces.hpp"
#include "sourcetrace/protocol/frame.hpp"

#include <array>
#include <cstddef>
#include <span>

namespace sourcetrace::crypto {

inline constexpr std::size_t kPublicKeySize = 32;
inline constexpr std::size_t kSecretKeySize = 64;
inline constexpr std::size_t kSeedSize = 32;

using PublicKey = std::array<std::byte, kPublicKeySize>;
using SecretKey = std::array<std::byte, kSecretKeySize>;
using Seed = std::array<std::byte, kSeedSize>;

struct KeyPair {
  PublicKey public_key;
  SecretKey secret_key;
};

class Ed25519Verifier final : public core::ICryptoVerifier {
public:
  explicit Ed25519Verifier(PublicKey public_key);
  [[nodiscard]] bool verify(std::span<const std::byte> message,
                            std::span<const std::byte> signature) const override;

private:
  PublicKey public_key_;
};

[[nodiscard]] KeyPair keyPairFromSeed(const Seed &seed);
[[nodiscard]] protocol::Signature signDetached(std::span<const std::byte> message,
                                               const SecretKey &secret_key);

} // namespace sourcetrace::crypto
