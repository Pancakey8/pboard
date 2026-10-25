#include "tool.hpp"
#include "net.hpp"
#include "shape.hpp"
#include <print>
#include <raylib.h>
#include <utility>

namespace pboard::tool {

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

    bool operator()(const shape::Bezier &bez) {
      constexpr int segments = 64;

      auto bezier_point = [&](float t) {
        float u = 1.0f - t;
        float b0 = u * u * u;
        float b1 = 3.0f * u * u * t;
        float b2 = 3.0f * u * t * t;
        float b3 = t * t * t;

        return std::pair{b0 * bez.x0 + b1 * bez.x1 + b2 * bez.x2 + b3 * bez.x3,
                         b0 * bez.y0 + b1 * bez.y1 + b2 * bez.y2 + b3 * bez.y3};
      };

      float radius = bez.thick / 2.0f;

      auto [prev_x, prev_y] = bezier_point(0.0f);

      for (int i = 1; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        auto [curr_x, curr_y] = bezier_point(t);

        float ab_x = curr_x - prev_x;
        float ab_y = curr_y - prev_y;
        float ap_x = x - prev_x;
        float ap_y = y - prev_y;

        float ab2 = ab_x * ab_x + ab_y * ab_y;

        float u = 0.0f;
        if (ab2 > 0.0f) {
          u = std::clamp((ap_x * ab_x + ap_y * ab_y) / ab2, 0.0f, 1.0f);
        }

        float dx = x - (prev_x + u * ab_x);
        float dy = y - (prev_y + u * ab_y);

        if (dx * dx + dy * dy <= radius * radius)
          return true;

        prev_x = curr_x;
        prev_y = curr_y;
      }

      return false;
    }
  } coll{x, y};

  return std::visit(coll, shape);
}

struct ToolVtor {
  net::ClientState &client;
  float vx, vy, scale;

  void operator()(PanTool &) {}

  void operator()(LineTool &tool) {
    switch (tool.status) {
    case ToolStatus::NONE: {
      if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        break;
      tool.x0 = GetMouseX() / scale + vx;
      tool.y0 = GetMouseY() / scale + vy;
      tool.status = ToolStatus::DRAW;
    } break;
    case ToolStatus::DRAW: {
      if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        break;
      tool.x1 = GetMouseX() / scale + vx;
      tool.y1 = GetMouseY() / scale + vy;
      tool.status = ToolStatus::FINAL;
    } break;
    case ToolStatus::FINAL:
      // TODO: Pivot editing
      break;
    }
  }

  void operator()(RectTool &tool) {
    switch (tool.status) {
    case ToolStatus::NONE: {
      if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        break;
      tool.x0 = GetMouseX() / scale + vx;
      tool.y0 = GetMouseY() / scale + vy;
      tool.status = ToolStatus::DRAW;
    } break;
    case ToolStatus::DRAW: {
      if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        break;
      tool.x1 = GetMouseX() / scale + vx;
      tool.y1 = GetMouseY() / scale + vy;
      tool.status = ToolStatus::FINAL;
    } break;
    case ToolStatus::FINAL:
      // TODO: Pivot editing
      break;
    }
  }

  void operator()(CircTool &tool) {
    switch (tool.status) {
    case ToolStatus::NONE: {
      if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        break;
      tool.x0 = GetMouseX() / scale + vx;
      tool.y0 = GetMouseY() / scale + vy;
      tool.status = ToolStatus::DRAW;
    } break;
    case ToolStatus::DRAW: {
      if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        break;
      tool.x1 = GetMouseX() / scale + vx;
      tool.y1 = GetMouseY() / scale + vy;
      tool.status = ToolStatus::FINAL;
    } break;
    case ToolStatus::FINAL:
      // TODO: Pivot editing
      break;
    }
  }

  void operator()(SelectTool &sel) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      auto mx = GetMouseX() / scale + vx;
      auto my = GetMouseY() / scale + vy;
      sel.selection = {};
      for (const auto &[id, obj] : std::views::reverse(client)) {
        if (point_in(mx, my, obj.shape)) {
          sel.selection = id;
          break;
        }
      }
    }

    if (sel.selection && IsKeyPressed(KEY_DELETE)) {
      client.erase(*sel.selection);
      sel.selection = {};
    }
  }

  void operator()(BezierTool &tool) {
    if (tool.idx == 8) {
      // TODO: Pivot editing
    } else {
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        tool.pts[tool.idx] = GetMouseX() / scale + vx;
        tool.pts[tool.idx + 1] = GetMouseY() / scale + vy;
        tool.idx += 2;
      }
      // TODO: Reintroduce dragging?
    }
  }
};

struct ConfirmVtor {
  float thick;
  shape::Color col;

  std::optional<shape::Shape> operator()(PanTool &) { return {}; }

  std::optional<shape::Shape> operator()(LineTool &tool) {
    if (tool.status != ToolStatus::FINAL) {
      return {};
    }

    tool.status = ToolStatus::NONE;
    return shape::Line{tool.x0, tool.y0, tool.x1, tool.y1, thick, col};
  }

  std::optional<shape::Shape> operator()(RectTool &tool) {
    if (tool.status != ToolStatus::FINAL) {
      return {};
    }

    tool.status = ToolStatus::NONE;
    auto [x0, x1] = std::minmax(tool.x0, tool.x1);
    auto [y0, y1] = std::minmax(tool.y0, tool.y1);

    return shape::Rect{x0, y0, x1 - x0, y1 - y0, col};
  }

  std::optional<shape::Shape> operator()(CircTool &tool) {
    if (tool.status != ToolStatus::FINAL) {
      return {};
    }

    tool.status = ToolStatus::NONE;

    float r = std::hypot(tool.x1 - tool.x0, tool.y1 - tool.y0);
    return shape::Circ{tool.x0, tool.y0, r, col};
  }

  std::optional<shape::Shape> operator()(SelectTool &) { return {}; }

  std::optional<shape::Shape> operator()(BezierTool &tool) {
    if (tool.idx != 8) {
      return {};
    }

    tool.idx = 0;

    return std::apply(
        [this](auto &&...pts) { return shape::Bezier{pts..., thick, col}; },
        tool.pts);
  }
};

// TODO: Reduce duplication
struct GhostVtor {
  float vx, vy, scale;
  float thick;
  shape::Color col;

  std::optional<shape::Shape> operator()(const PanTool &) { return {}; }

  std::optional<shape::Shape> operator()(const LineTool &tool) {
    if (tool.status == ToolStatus::NONE) {
      return {};
    }

    float xm{tool.x1}, ym{tool.y1};

    if (tool.status == ToolStatus::DRAW) {
      xm = GetMouseX() / scale + vx;
      ym = GetMouseY() / scale + vy;
    }

    return shape::Line{tool.x0, tool.y0, xm, ym, thick, col};
  }

  std::optional<shape::Shape> operator()(const RectTool &tool) {
    if (tool.status == ToolStatus::NONE) {
      return {};
    }

    float xm{tool.x1}, ym{tool.y1};

    if (tool.status == ToolStatus::DRAW) {
      xm = GetMouseX() / scale + vx;
      ym = GetMouseY() / scale + vy;
    }

    auto [x0, x1] = std::minmax(tool.x0, xm);
    auto [y0, y1] = std::minmax(tool.y0, ym);

    return shape::Rect{x0, y0, x1 - x0, y1 - y0, col};
  }

  std::optional<shape::Shape> operator()(const CircTool &tool) {
    if (tool.status == ToolStatus::NONE) {
      return {};
    }

    float xm{tool.x1}, ym{tool.y1};

    if (tool.status == ToolStatus::DRAW) {
      xm = GetMouseX() / scale + vx;
      ym = GetMouseY() / scale + vy;
    }

    float r = std::hypot(xm - tool.x0, ym - tool.y0);
    return shape::Circ{tool.x0, tool.y0, r, col};
  }

  std::optional<shape::Shape> operator()(const SelectTool &) { return {}; }

  std::optional<shape::Shape> operator()(const BezierTool &tool) {
    if (tool.idx == 0) {
      return {};
    }

    float xm = GetMouseX() / scale + vx;
    float ym = GetMouseY() / scale + vy;

    auto pts = tool.pts;

    for (int i = tool.idx; i < 8; i += 2) {
      pts[i] = xm;
      pts[i + 1] = ym;
    }

    return std::apply(
        [this](auto &&...pts) { return shape::Bezier{pts..., thick, col}; },
        pts);
  }
};

std::optional<shape::Shape> ToolState::update(net::ClientState &client,
                                              float vx, float vy, float scale) {
  std::visit(ToolVtor{client, vx, vy, scale}, data);

  if (IsKeyPressed(KEY_ENTER)) {
    return std::visit(ConfirmVtor{line_thickness, color}, data);
  }

  return {};
}

std::optional<shape::Shape> ToolState::ghost(float vx, float vy,
                                             float scale) const {
  return std::visit(GhostVtor{vx, vy, scale, line_thickness, color}, data);
}

} // namespace pboard::tool
