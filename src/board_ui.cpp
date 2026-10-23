#include "board_ui.hpp"
#include "net.hpp"
#include "protocol.hpp"
#include "uuid.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>

namespace pboard::board_ui {

std::optional<shape::Shape> ToolUse::render_of(float xm, float ym) const {
  switch (selection) {
  case Tool::PAN:
    break;
  case Tool::LINE: {
    shape::Line line{x0, y0, xm, ym, line_thickness, color};
    if (std::hypot(line.x1 - line.x0, line.y1 - line.y0) > MIN_SIZE)
      return line;
  } break;
  case Tool::RECT: {
    auto [x0, x1] = std::minmax(this->x0, xm);
    auto [y0, y1] = std::minmax(this->y0, ym);
    if ((y1 - y0) > MIN_SIZE && (x1 - x0) > MIN_SIZE)
      return shape::Rect{x0, y0, x1 - x0, y1 - y0, color};
  } break;
  case Tool::CIRC: {
    float r = std::hypot(xm - x0, ym - y0);
    if (r > MIN_SIZE)
      return shape::Circ{x0, y0, r, color};
  } break;
  case Tool::SELECT:
    break;
  }

  return {};
}

bool point_in(float x, float y, const shape::Shape &shape) {
  struct {
    float x, y;

    bool operator()(const shape::Line &line) {
      float ab_x = line.x1 - line.x0;
      float ab_y = line.y1 - line.y0;

      float ap_x = x - line.x0;
      float ap_y = y - line.y0;

      float ab2 = ab_x * ab_x + ab_y * ab_y;

      float t = 0.0f;
      if (ab2 != 0) {
        t = (ap_x * ab_x + ap_y * ab_y) / ab2;
        t = std::clamp(t, 0.0f, 1.0f);
      }

      float c_x = line.x0 + t * ab_x;
      float c_y = line.y0 + t * ab_y;

      float d = std::hypot(x - c_x, y - c_y);
      float r = line.thick / 2;

      return d <= r;
    }

    bool operator()(const shape::Rect &rect) {
      return (rect.x <= x && x <= rect.x + rect.w) &&
             (rect.y <= y && y <= rect.y + rect.h);
    }

    bool operator()(const shape::Circ &circ) {
      auto d = std::hypot(circ.x - x, circ.y - y);
      return d <= circ.r;
    }
  } coll{x, y};

  return std::visit(coll, shape);
}

void UIState::update() {
  if (IsWindowResized()) {
    layout();
  }

  peers.update(client);

  if (!active())
    return;

  if (IsKeyPressed(KEY_M)) {
    tool.selection = ToolUse::Tool::PAN;
  } else if (IsKeyPressed(KEY_R)) {
    tool.selection = ToolUse::Tool::RECT;
  } else if (IsKeyPressed(KEY_C)) {
    tool.selection = ToolUse::Tool::CIRC;
  } else if (IsKeyPressed(KEY_L)) {
    tool.selection = ToolUse::Tool::LINE;
  } else if (IsKeyPressed(KEY_S)) {
    tool.selection = ToolUse::Tool::SELECT;
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    tool.x0 = GetMouseX() / scale + vx;
    tool.y0 = GetMouseY() / scale + vy;
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
      tool.selection == ToolUse::Tool::SELECT) {
    bool found{false};
    for (const auto &[id, obj] : std::views::reverse(client)) {
      if (point_in(tool.x0, tool.y0, obj.shape)) {
        found = true;
        shape_targ = id;
        break;
      }
    }

    if (!found)
      shape_targ = {};
  }

  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    if (auto shape =
            tool.render_of(GetMouseX() / scale + vx, GetMouseY() / scale + vy))
      client.push_back(shape.value(), client.last);
  }

  if (tool.selection == ToolUse::Tool::PAN &&
      IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    auto dv = GetMouseDelta();
    vx -= dv.x / scale;
    vy -= dv.y / scale;
  }

  if (shape_targ && IsKeyPressed(KEY_DELETE)) {
    client.erase(shape_targ.value());
    shape_targ = {};
  }

  if (tool.selection == ToolUse::Tool::PAN && GetMouseWheelMove() != 0) {
    scale += GetMouseWheelMove() * 0.2f;
    scale = std::clamp(scale, 0.1f, 3.0f);
  }

  if (tool.selection == ToolUse::Tool::LINE && GetMouseWheelMove() != 0) {
    tool.line_thickness += GetMouseWheelMove() * 2.0f;
    tool.line_thickness = std::max(1.0f, tool.line_thickness);
  }

  if (IsKeyPressed(KEY_ONE)) {
    tool.color = {0, 0, 0};
  }

  if (IsKeyPressed(KEY_TWO)) {
    tool.color = {255, 0, 0};
  }

  if (IsKeyPressed(KEY_THREE)) {
    tool.color = {0, 255, 0};
  }

  if (IsKeyPressed(KEY_FOUR)) {
    tool.color = {0, 0, 255};
  }

  bool cursor_send = (!cursor.timer.active || cursor.timer.expired());
  auto mouse = GetMousePosition();
  float mx = mouse.x / scale + vx;
  float my = mouse.y / scale + vy;
  bool cursor_moved = std::hypot(mx - cursor.xp, my - cursor.yp) > 5.0f;
  if (cursor_send && cursor_moved) {
    cursor.timer = Timer{net::Cursor::DELTA};
    cursor.xp = mx;
    cursor.yp = my;
    client.send_cursor(mx, my);
  }
}

Color color_of_id(const uuids::uuid &id) {
  const auto &b = id.as_bytes();
  std::uint64_t left{}, right{};
  std::memcpy(&left, b.data(), 8);
  std::memcpy(&right, b.data() + 8, 8);

  float h = static_cast<float>((left % 720) * 275 % 720) / 2.0f;
  float s = (55 + right % 31) / 100.0f;
  float v = (70 + (right >> 8) % 26) / 100.0f;

  return ColorFromHSV(h, s, v);
}

void UIState::draw_cursor(float x, float y, net::ClientId id,
                          bool can_offscreen) const {
  float w = GetRenderWidth();
  float h = GetRenderHeight();

  constexpr float SIZE = 8.0f;
  constexpr float MARGIN = 16.0f;

  bool outside_region =
      (x < MARGIN || x > w - MARGIN || y < MARGIN || y > h - MARGIN);

  if (can_offscreen && outside_region) {
    float cx = std::clamp(x, MARGIN, w - MARGIN);
    float cy = std::clamp(y, MARGIN, h - MARGIN);

    float dx = x - cx;
    float dy = y - cy;
    float len = std::max(1e-4f, std::hypot(dx, dy));

    float dir_x = dx / len;
    float dir_y = dy / len;

    float perp_x = -dir_y;
    float perp_y = dir_x;

    Vector2 tip{cx + dir_x * SIZE, cy + dir_y * SIZE};

    Vector2 base{cx - dir_x * SIZE * 0.5f, cy - dir_y * SIZE * 0.5f};
    Vector2 basel{base.x + perp_x * SIZE * 0.8f, base.y + perp_y * SIZE * 0.8f};
    Vector2 baser{base.x - perp_x * SIZE * 0.8f, base.y - perp_y * SIZE * 0.8f};

    DrawTriangle(tip, baser, basel, color_of_id(id));
  } else {
    DrawCircle(x, y, SIZE, color_of_id(id));
  }
}

void UIState::render() const {
  static constexpr Color HL_COLOR = Color(235, 98, 77, 170);

  struct RendVtor {
    float vx, vy;
    float scale;
    float alpha;
    bool is_hl{false};

    void operator()(const shape::Line &line) {
      if (is_hl) {
        float ab_x = line.x1 - line.x0;
        float ab_y = line.y1 - line.y0;
        float ab = std::hypot(ab_x, ab_y);
        float dx = 2 * ab_x / ab;
        float dy = 2 * ab_y / ab;

        DrawLineEx({(line.x0 - vx - dx) * scale, (line.y0 - vy - dy) * scale},
                   {(line.x1 - vx + dx) * scale, (line.y1 - vy + dy) * scale},
                   (line.thick + 4.0f) * scale, HL_COLOR);
      }

      DrawLineEx({(line.x0 - vx) * scale, (line.y0 - vy) * scale},
                 {(line.x1 - vx) * scale, (line.y1 - vy) * scale},
                 line.thick * scale,
                 Color(line.col.r, line.col.g, line.col.b, alpha));
    }

    void operator()(const shape::Rect &rect) {
      if (is_hl)
        DrawRectangle((rect.x - vx - 2.0f) * scale,
                      (rect.y - vy - 2.0f) * scale, (rect.w + 4.0f) * scale,
                      (rect.h + 4.0f) * scale, HL_COLOR);

      DrawRectangle((rect.x - vx) * scale, (rect.y - vy) * scale,
                    rect.w * scale, rect.h * scale,
                    Color(rect.col.r, rect.col.g, rect.col.b, alpha));
    }

    void operator()(const shape::Circ &circ) {
      if (is_hl)
        DrawCircle((circ.x - vx) * scale, (circ.y - vy) * scale,
                   (circ.r + 2.0f) * scale, HL_COLOR);

      DrawCircle((circ.x - vx) * scale, (circ.y - vy) * scale, circ.r * scale,
                 Color(circ.col.r, circ.col.g, circ.col.b, alpha));
    }
  };

  ClearBackground(WHITE);

  RendVtor rend{vx, vy, scale, 255}, hl{vx, vy, scale, 255, true};
  for (const auto &[id, obj] : client) {
    if (shape_targ == id) {
      std::visit(hl, obj.shape);
    } else {
      std::visit(rend, obj.shape);
    }
  }

  RendVtor ghost{vx, vy, scale, 170};
  if (active() && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    if (auto shape = tool.render_of(GetMouseX() / scale + vx,
                                    GetMouseY() / scale + vy)) {
      std::visit(ghost, shape.value());
    }
  }

  for (const auto &[id, cur] : client.cursors) {
    if (!cur.alive())
      continue;
    const auto &[x, y] = cur.pos();
    draw_cursor((x - vx) * scale, (y - vy) * scale, id, true);
  }

  peers.render(ui_ctx);

  auto cur = GetMousePosition();
  draw_cursor(cur.x, cur.y, client.self, false);
}

void UIState::layout() {
  // float w = GetRenderWidth();
  float h = GetRenderHeight();
  peers.x = 20;
  peers.y = h - 200.0f - peers.BORDER;
  peers.w = 250.0f;
  peers.h = 200.0f;
}

bool UIState::active() const { return !peers.is_active; }

}; // namespace pboard::board_ui
