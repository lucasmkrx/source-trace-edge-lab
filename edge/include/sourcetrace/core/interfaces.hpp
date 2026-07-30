#pragma once

#include "sourcetrace/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace sourcetrace::core {

class IClock {
public:
  virtual ~IClock() = default;
  [[nodiscard]] virtual std::uint64_t monotonicMs() const = 0;
};

class IStorage {
public:
  virtual ~IStorage() = default;
  [[nodiscard]] virtual bool persistCheckpoint(const ControllerState &state) = 0;
  [[nodiscard]] virtual std::optional<ControllerState> loadCheckpoint() = 0;
  [[nodiscard]] virtual bool enqueue(std::span<const std::byte> event) = 0;
};

class ITransport {
public:
  virtual ~ITransport() = default;
  [[nodiscard]] virtual bool connected() const = 0;
  [[nodiscard]] virtual bool send(std::span<const std::byte> frame) = 0;
};

class ICryptoVerifier {
public:
  virtual ~ICryptoVerifier() = default;
  [[nodiscard]] virtual bool verify(std::span<const std::byte> message,
                                    std::span<const std::byte> signature) const = 0;
};

class IInputs {
public:
  virtual ~IInputs() = default;
  [[nodiscard]] virtual std::optional<Event> poll() = 0;
};

class IOutputs {
public:
  virtual ~IOutputs() = default;
  virtual void setIndicator(Indicator indicator) = 0;
};

class IAuditSink {
public:
  virtual ~IAuditSink() = default;
  [[nodiscard]] virtual bool append(AuditCode code, std::uint64_t at_ms,
                                    std::string_view detail) = 0;
};

} // namespace sourcetrace::core
