#pragma once

#include <optional>
#include <variant>
#include <vector>

namespace pboard::ui {
struct RenderColor {
  float r, g, b;
};

struct RenderLine {
  float x0, y0, x1, y1;
  float thick;
  RenderColor col;
};

struct RenderRect {
  float x, y, w, h;
  RenderColor col;
};

struct RenderCirc {
  float x, y, r;
  RenderColor col;
};

constexpr float MIN_SIZE = 10;

using RenderShape = std::variant<RenderLine, RenderRect, RenderCirc>;

struct ToolUse {
  enum class Tool { PAN, LINE, RECT, CIRC } selection{Tool::PAN};
  float line_thickness{10.0f};
  RenderColor color{0, 0, 0};
  float x0{0}, y0{0};

  std::optional<RenderShape> render_of(float xm, float ym) const;
};

struct UIState {
  ToolUse tool{};
  std::vector<RenderShape> shapes{};
  float vx, vy;

  void update();
  void render() const;
};
}; // namespace pboard::ui
