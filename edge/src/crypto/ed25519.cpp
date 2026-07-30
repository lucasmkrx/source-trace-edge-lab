#include "sourcetrace/crypto/ed25519.hpp"

#include <sodium.h>

#include <stdexcept>

namespace sourcetrace::crypto {
namespace {

void ensureSodium() {
  static const int initialized = sodium_init();
  if (initialized < 0) {
    throw std::runtime_error("libsodium initialization failed");
  }
}

const unsigned char *asUnsigned(std::span<const std::byte> bytes) {
  return reinterpret_cast<const unsigned char *>(bytes.data());
}

unsigned char *asUnsignedWritable(std::span<std::byte> bytes) {
  return reinterpret_cast<unsigned char *>(bytes.data());
}

} // namespace

Ed25519Verifier::Ed25519Verifier(PublicKey public_key) : public_key_(public_key) {
  ensureSodium();
}

bool Ed25519Verifier::verify(std::span<const std::byte> message,
                             std::span<const std::byte> signature) const {
  if (signature.size() != protocol::kSignatureSize) {
    return false;
  }
  return crypto_sign_verify_detached(asUnsigned(signature), asUnsigned(message), message.size(),
                                     asUnsigned(public_key_)) == 0;
}

KeyPair keyPairFromSeed(const Seed &seed) {
  ensureSodium();
  KeyPair pair{};
  if (crypto_sign_seed_keypair(
          asUnsignedWritable(std::span<std::byte>(pair.public_key)),
          asUnsignedWritable(std::span<std::byte>(pair.secret_key)),
          asUnsigned(std::span<const std::byte>(seed))) != 0) {
    throw std::runtime_error("Ed25519 key generation failed");
  }
  return pair;
}

protocol::Signature signDetached(std::span<const std::byte> message,
                                 const SecretKey &secret_key) {
  ensureSodium();
  protocol::Signature signature{};
  unsigned long long signature_length = 0;
  if (crypto_sign_detached(
          asUnsignedWritable(std::span<std::byte>(signature)), &signature_length,
          asUnsigned(message), message.size(),
          asUnsigned(std::span<const std::byte>(secret_key))) != 0 ||
      signature_length != signature.size()) {
    throw std::runtime_error("Ed25519 signing failed");
  }
  return signature;
}

} // namespace sourcetrace::crypto
