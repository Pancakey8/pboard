#include "ui.hpp"
#include <algorithm>
#include <cmath>
#include <raylib.h>

namespace pboard::ui {

std::optional<RenderShape> ToolUse::render_of(float xm, float ym) const {
  switch (selection) {
  case Tool::PAN:
    break;
  case Tool::LINE: {
    RenderLine line{x0, y0, xm, ym, line_thickness, color};
    if (std::hypot(line.x1 - line.x0, line.y1 - line.y0) > MIN_SIZE)
      return line;
  } break;
  case Tool::RECT: {
    auto [x0, x1] = std::minmax(this->x0, xm);
    auto [y0, y1] = std::minmax(this->y0, ym);
    if ((y1 - y0) > MIN_SIZE && (x1 - x0) > MIN_SIZE)
      return RenderRect{x0, y0, x1 - x0, y1 - y0, color};
  } break;
  case Tool::CIRC: {
    float r = std::hypot(xm - x0, ym - y0);
    if (r > MIN_SIZE)
      return RenderCirc{x0, y0, r, color};
  } break;
  }

  return {};
}

void UIState::update() {
  if (IsKeyPressed(KEY_M)) {
    tool.selection = ToolUse::Tool::PAN;
  } else if (IsKeyPressed(KEY_R)) {
    tool.selection = ToolUse::Tool::RECT;
  } else if (IsKeyPressed(KEY_C)) {
    tool.selection = ToolUse::Tool::CIRC;
  } else if (IsKeyPressed(KEY_L)) {
    tool.selection = ToolUse::Tool::LINE;
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    tool.x0 = GetMouseX() / scale + vx;
    tool.y0 = GetMouseY() / scale + vy;
  }

  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    if (auto shape = tool.render_of(GetMouseX() / scale + vx, GetMouseY() / scale + vy))
      shapes.emplace_back(shape.value());
  }

  if (tool.selection == ToolUse::Tool::PAN &&
      IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    auto dv = GetMouseDelta();
    vx -= dv.x / scale;
    vy -= dv.y / scale;
  }

  if (tool.selection == ToolUse::Tool::PAN && GetMouseWheelMove() != 0) {
    scale += GetMouseWheelMove() * 0.2f;
    scale = std::clamp(scale, 0.1f, 3.0f);
  }

  if (tool.selection == ToolUse::Tool::LINE &&
      GetMouseWheelMove() != 0) {
    tool.line_thickness += GetMouseWheelMove() * 2.0f;
    tool.line_thickness = std::max(1.0f, tool.line_thickness);
  }

  if (tool.selection != ToolUse::Tool::PAN && IsKeyPressed(KEY_ONE)) {
    tool.color = {0, 0, 0};
  }

  if (tool.selection != ToolUse::Tool::PAN && IsKeyPressed(KEY_TWO)) {
    tool.color = {255, 0, 0};
  }

  if (tool.selection != ToolUse::Tool::PAN && IsKeyPressed(KEY_THREE)) {
    tool.color = {0, 255, 0};
  }

  if (tool.selection != ToolUse::Tool::PAN && IsKeyPressed(KEY_FOUR)) {
    tool.color = {0, 0, 255};
  }
}

void UIState::render() const {
  struct {
    float vx, vy;
    float scale;
    float alpha;

    void operator()(const RenderLine &line) {
      DrawLineEx({(line.x0 - vx) * scale, (line.y0 - vy) * scale},
                 {(line.x1 - vx) * scale, (line.y1 - vy) * scale},
                 line.thick * scale,
                 Color(line.col.r, line.col.g, line.col.b, alpha));
    }

    void operator()(const RenderRect &rect) {
      DrawRectangle((rect.x - vx) * scale, (rect.y - vy) * scale,
                    rect.w * scale, rect.h * scale,
                    Color(rect.col.r, rect.col.g, rect.col.b, alpha));
    }

    void operator()(const RenderCirc &circ) {
      DrawCircle((circ.x - vx) * scale, (circ.y - vy) * scale, circ.r * scale,
                 Color(circ.col.r, circ.col.g, circ.col.b, alpha));
    }
  } rend{vx, vy, scale, 255}, ghost{vx, vy, scale, 170};

  ClearBackground(WHITE);

  for (const auto &shape : shapes) {
    std::visit(rend, shape);
  }

  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    if (auto shape = tool.render_of(GetMouseX() / scale + vx, GetMouseY() / scale + vy)) {
      std::visit(ghost, shape.value());
    }
  }
}

}; // namespace pboard::ui
