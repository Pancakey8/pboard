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
    tool.x0 = GetMouseX() + vx;
    tool.y0 = GetMouseY() + vy;
  }

  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    if (auto shape = tool.render_of(GetMouseX() + vx, GetMouseY() + vy))
      shapes.emplace_back(shape.value());
  }

  if (tool.selection == ToolUse::Tool::PAN &&
      IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    auto dv = GetMouseDelta();
    vx -= dv.x;
    vy -= dv.y;
  }

  if (tool.selection == ToolUse::Tool::LINE &&
      IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
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
    float alpha;

    void operator()(const RenderLine &line) {
      DrawLineEx({line.x0 - vx, line.y0 - vy}, {line.x1 - vx, line.y1 - vy},
                 line.thick, Color(line.col.r, line.col.g, line.col.b, alpha));
    }

    void operator()(const RenderRect &rect) {
      DrawRectangle(rect.x - vx, rect.y - vy, rect.w, rect.h,
                    Color(rect.col.r, rect.col.g, rect.col.b, alpha));
    }

    void operator()(const RenderCirc &circ) {
      DrawCircle(circ.x - vx, circ.y - vy, circ.r,
                 Color(circ.col.r, circ.col.g, circ.col.b, alpha));
    }
  } rend{vx, vy, 255}, ghost{vx, vy, 170};

  ClearBackground(WHITE);

  for (const auto &shape : shapes) {
    std::visit(rend, shape);
  }

  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    if (auto shape = tool.render_of(GetMouseX() + vx, GetMouseY() + vy)) {
      std::visit(ghost, shape.value());
    }
  }
}

}; // namespace pboard::ui
