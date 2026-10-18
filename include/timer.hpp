#pragma once

#include <chrono>

struct Timer {
  std::chrono::steady_clock::time_point expires_at;
  bool active = false;

  Timer() = default;

  Timer(std::chrono::steady_clock::duration d)
      : expires_at{std::chrono::steady_clock::now() + d}, active{true} {}

  bool expired() const {
    return std::chrono::steady_clock::now() >= expires_at;
  }
};
