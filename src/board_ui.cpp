#include "board_ui.hpp"
#include "net.hpp"
#include "protocol.hpp"
#include "shape.hpp"
#include "tool.hpp"
#include "uuid.h"
#include "widget.hpp"
#include <algorithm>
#include <cmath>
#include <print>
#include <raylib.h>
#include <type_traits>

namespace pboard::board_ui {

// shape::Bezier make_bezier(const tool::ToolState &tool, float mx, float my) {
//   auto &pts = tool.bez.pts;
//   auto idx = tool.bez.idx;

//   if (idx == 8) {
//     return {pts[0],    pts[1], pts[2],
//             pts[3],    pts[4], pts[5],
//             pts[6],    pts[7], tool.line_thickness,
//             tool.color};
//   }

//   if (idx == 0) {
//     float dx = mx - tool.x0;
//     float dy = my - tool.y0;

//     return {tool.x0,
//             tool.y0,
//             tool.x0 + dx / 3.0f,
//             tool.y0 + dy / 3.0f,
//             tool.x0 + 2.0f * dx / 3.0f,
//             tool.y0 + 2.0f * dy / 3.0f,
//             mx,
//             my,
//             tool.line_thickness,
//             tool.color};
//   }

//   float p[8];
//   for (std::uint64_t i = 0; i < idx; ++i)
//     p[i] = pts[i];

//   for (std::uint64_t i = idx; i < 8; i += 2) {
//     p[i] = mx;
//     p[i + 1] = my;
//   }

//   return {
//       p[0],      p[1], p[2], p[3], p[4], p[5], p[6], p[7],
//       tool.line_thickness, tool.color};
// }

// std::optional<shape::Shape> tool_shape(const tool::ToolState &tool, float xm,
//                                        float ym) {
//   switch (tool.selection) {
//   case tool::Tool::PAN:
//     break;
//   case tool::Tool::LINE: {
//     shape::Line line{tool.x0, tool.y0, xm, ym, tool.line_thickness,
//     tool.color}; if (std::hypot(line.x1 - line.x0, line.y1 - line.y0) >
//     MIN_SIZE)
//       return line;
//   } break;
//   case tool::Tool::RECT: {
//     auto [x0, x1] = std::minmax(tool.x0, xm);
//     auto [y0, y1] = std::minmax(tool.y0, ym);
//     if ((y1 - y0) > MIN_SIZE && (x1 - x0) > MIN_SIZE)
//       return shape::Rect{x0, y0, x1 - x0, y1 - y0, tool.color};
//   } break;
//   case tool::Tool::CIRC: {
//     float r = std::hypot(xm - tool.x0, ym - tool.y0);
//     if (r > MIN_SIZE)
//       return shape::Circ{tool.x0, tool.y0, r, tool.color};
//   } break;
//   case tool::Tool::SELECT:
//     break;
//   case tool::Tool::BEZIER: {
//     return make_bezier(tool, xm, ym);
//   } break;
//   }

//   return {};
// }

void UIState::update() {
  if (IsWindowResized()) {
    layout();
  }

  peers.update(client);
  tool_menu.update(tool);

  if (!active())
    return;

  if (IsKeyPressed(KEY_M)) {
    tool.selection = tool::Tool::PAN;
    tool.data = tool::PanTool{};
  } else if (IsKeyPressed(KEY_R)) {
    tool.selection = tool::Tool::RECT;
    tool.data = tool::RectTool{};
  } else if (IsKeyPressed(KEY_C)) {
    tool.selection = tool::Tool::CIRC;
    tool.data = tool::CircTool{};
  } else if (IsKeyPressed(KEY_L)) {
    tool.selection = tool::Tool::LINE;
    tool.data = tool::LineTool{};
  } else if (IsKeyPressed(KEY_S)) {
    tool.selection = tool::Tool::SELECT;
    tool.data = tool::SelectTool{};
  } else if (IsKeyPressed(KEY_B)) {
    tool.selection = tool::Tool::BEZIER;
    tool.data = tool::BezierTool{};
  }

  if (auto shape = tool.update(client, vx, vy, scale)) {
    client.push_back(*shape, client.last);
  }

  if (tool.selection == tool::Tool::PAN &&
      IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    auto dv = GetMouseDelta();
    vx -= dv.x / scale;
    vy -= dv.y / scale;
  }

  if (tool.selection == tool::Tool::PAN && GetMouseWheelMove() != 0) {
    scale += GetMouseWheelMove() * 0.2f;
    scale = std::clamp(scale, 0.1f, 3.0f);
  }

  if (tool.selection != tool::Tool::PAN && GetMouseWheelMove() != 0) {
    tool.line_thickness += GetMouseWheelMove() * 2.0f;
    tool.line_thickness = std::max(1.0f, tool.line_thickness);
  }

  static_assert(std::tuple_size_v<decltype(tool_menu.hotbar)> <= 9);
  for (std::underlying_type_t<KeyboardKey> key = KEY_ONE; key <= KEY_NINE;
       ++key) {
    auto n = key - KEY_ONE;
    if (!IsKeyPressed(key))
      continue;
    if (n >= tool_menu.hotbar.size())
      break;
    tool.color = {static_cast<float>(tool_menu.hotbar[n].r),
                  static_cast<float>(tool_menu.hotbar[n].g),
                  static_cast<float>(tool_menu.hotbar[n].b)};
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
                   (line.thick + 2.0f) * scale, HL_COLOR);
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

    void operator()(const shape::Bezier &bez) {
// weird?
#define TX(x) ((x) - vx) * scale
#define TY(y) ((y) - vy) * scale
      if (is_hl)
        DrawSplineSegmentBezierCubic(
            {TX(bez.x0), TY(bez.y0)}, {TX(bez.x1), TY(bez.y1)},
            {TX(bez.x2), TY(bez.y2)}, {TX(bez.x3), TY(bez.y3)},
            (bez.thick + 2.0f) * scale, HL_COLOR);

      DrawSplineSegmentBezierCubic(
          {TX(bez.x0), TY(bez.y0)}, {TX(bez.x1), TY(bez.y1)},
          {TX(bez.x2), TY(bez.y2)}, {TX(bez.x3), TY(bez.y3)}, bez.thick * scale,
          Color(bez.col.r, bez.col.g, bez.col.b, alpha));
#undef TX
#undef TY
    }
  };

  ClearBackground(WHITE);

  RendVtor rend{vx, vy, scale, 255}, hl{vx, vy, scale, 255, true};

  std::optional<net::ObjectId> targ{};
  if (tool.selection == tool::Tool::SELECT) {
    targ = std::get<tool::SelectTool>(tool.data).selection;
  }

  for (const auto &[id, obj] : client) {
    if (targ == id) {
      std::visit(hl, obj.shape);
    } else {
      std::visit(rend, obj.shape);
    }
  }

  RendVtor ghost{vx, vy, scale, 170};
  if (auto s = tool.ghost(vx, vy, scale)) {
    std::visit(ghost, *s);
  }

  for (const auto &[id, cur] : client.cursors) {
    if (!cur.alive())
      continue;
    const auto &[x, y] = cur.pos();
    draw_cursor((x - vx) * scale, (y - vy) * scale, id, true);
  }

  peers.render(ui_ctx);

  tool_menu.render(tool, ui_ctx);

  auto cur = GetMousePosition();
  draw_cursor(cur.x, cur.y, client.self, false);
}

void UIState::layout() {
  float w = GetRenderWidth();
  float h = GetRenderHeight();
  peers.x = 20.0f;
  peers.y = h - 200.0f - peers.BORDER;
  peers.w = 250.0f;
  peers.h = 200.0f;

  tool_menu.x = w - 100.0f;
  tool_menu.y = tool_menu.BORDER;
  tool_menu.w = 100.0f - tool_menu.BORDER;
  tool_menu.h = h - 2.0f * tool_menu.BORDER;
}

bool UIState::active() const {
  return !peers.is_active && !tool_menu.is_active;
}

}; // namespace pboard::board_ui
