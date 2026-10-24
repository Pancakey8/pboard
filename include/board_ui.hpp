#pragma once

#include "net.hpp"
#include "protocol.hpp"
#include "shape.hpp"
#include "timer.hpp"
#include "tool.hpp"
#include "widget.hpp"
#include <optional>

namespace pboard::board_ui {
constexpr float MIN_SIZE = 10;

struct CursorState {
  Timer timer;
  float xp, yp;
};

struct UIState {
  const widget::UIContext &ui_ctx;

  tool::ToolState tool{};
  net::ClientState &client;
  std::optional<net::ObjectId> shape_targ{};
  float vx{0.0f}, vy{0.0f};
  float scale{1.0f};

  CursorState cursor;

  widget::PeerList peers{};

  widget::ToolMenu tool_menu{};

  explicit UIState(const widget::UIContext &ui_ctx, net::ClientState &client)
      : ui_ctx{ui_ctx}, client{client} {
    auto &base = tool_menu.hotbar[0];
    tool.color = {static_cast<float>(base.r), static_cast<float>(base.g),
                  static_cast<float>(base.b)};
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
