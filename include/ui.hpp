#pragma once

#include "net.hpp"
#include "shape.hpp"
#include <optional>

namespace pboard::ui {
constexpr float MIN_SIZE = 10;

struct ToolUse {
  enum class Tool { PAN, LINE, RECT, CIRC, SELECT } selection{Tool::PAN};
  float line_thickness{10.0f};
  shape::Color color{0, 0, 0};
  float x0{0}, y0{0};

  std::optional<shape::Shape> render_of(float xm, float ym) const;
};

struct UIState {
  ToolUse tool{};
  net::ClientState &client;
  std::optional<net::ObjectId> shape_targ{};
  float vx{0.0f}, vy{0.0f};
  float scale{1.0f};

  explicit UIState(net::ClientState &client) : client{client} {}

  void update();
  void render() const;
};
}; // namespace pboard::ui
