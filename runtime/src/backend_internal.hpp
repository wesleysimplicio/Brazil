#pragma once

#include <memory>
#include <string>

#include "us4/backend.hpp"

namespace us4::detail {

// Real compute backends (portable; NEON/Accelerate path on Apple ARM).
std::unique_ptr<IBackend> make_generic_cpu();
std::unique_ptr<IBackend> make_neon_accelerate(bool available);

// Declared-but-not-yet-integrated accelerators. They report unavailable with a
// reason so the selector documents the routing contract and falls back safely.
std::unique_ptr<IBackend> make_planned(BackendKind kind, std::string reason);

}  // namespace us4::detail
