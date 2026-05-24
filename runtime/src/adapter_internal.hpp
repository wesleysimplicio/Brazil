#pragma once

#include <memory>
#include <vector>

#include "us4/adapter.hpp"

namespace us4::detail {

std::vector<std::unique_ptr<IUS4V6Adapter>> make_builtin_adapters();

}  // namespace us4::detail
