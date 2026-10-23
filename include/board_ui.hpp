#pragma once

#include "net.hpp"
#include "protocol.hpp"
#include "shape.hpp"
#include "timer.hpp"
#include "widget.hpp"
#include <optional>

namespace pboard::board_ui {
constexpr float MIN_SIZE = 10;

struct ToolUse {
  enum class Tool { PAN, LINE, RECT, CIRC, SELECT } selection{Tool::PAN};
  float line_thickness{10.0f};
  shape::Color color{0, 0, 0};
  float x0{0}, y0{0};

  std::optional<shape::Shape> render_of(float xm, float ym) const;
};

struct CursorState {
  Timer timer;
  float xp, yp;
};

struct UIState {
  const widget::UIContext &ui_ctx;

  ToolUse tool{};
  net::ClientState &client;
  std::optional<net::ObjectId> shape_targ{};
  float vx{0.0f}, vy{0.0f};
  float scale{1.0f};

  CursorState cursor;

  widget::PeerList peers{};

  explicit UIState(const widget::UIContext &ui_ctx, net::ClientState &client)
      : ui_ctx{ui_ctx}, client{client} {
    layout();
  }

  void draw_cursor(float x, float y, net::ClientId id,
                   bool can_offscreen) const;

  void layout();
  void update();
  void render() const;

  bool active() const;
};
}; // namespace pboard::board_ui
