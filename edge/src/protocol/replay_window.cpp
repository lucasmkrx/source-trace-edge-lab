#include "sourcetrace/protocol/replay_window.hpp"

#include <stdexcept>

namespace sourcetrace::protocol {

ReplayDecision ReplayWindow::evaluate(const Frame &frame) const {
  if (!frame.signature) {
    throw std::invalid_argument("replay decisions require an authenticated frame");
  }
  if (!sequence_) {
    return ReplayDecision::accept_new;
  }
  if (frame.sequence > *sequence_) {
    return ReplayDecision::accept_new;
  }
  if (frame.sequence < *sequence_) {
    return ReplayDecision::replay;
  }
  return frame.signature == signature_ ? ReplayDecision::duplicate : ReplayDecision::conflict;
}

void ReplayWindow::commit(const Frame &frame) {
  if (!frame.signature) {
    throw std::invalid_argument("cannot commit an unauthenticated replay record");
  }
  if (evaluate(frame) != ReplayDecision::accept_new) {
    throw std::invalid_argument("only a new authenticated sequence can be committed");
  }
  sequence_ = frame.sequence;
  signature_ = frame.signature;
}

void ReplayWindow::restore(std::uint64_t sequence, const Signature &signature) {
  sequence_ = sequence;
  signature_ = signature;
}

std::optional<std::uint64_t> ReplayWindow::highWaterMark() const { return sequence_; }

} // namespace sourcetrace::protocol
