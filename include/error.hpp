#pragma once

#include <string>

namespace pecco {

// Common error structure used across compilation phases
struct Error {
  std::string message;
  size_t line;
  size_t column;

  Error(std::string msg, size_t l = 0, size_t c = 0)
      : message(std::move(msg)), line(l), column(c) {}
};

} // namespace pecco
