#pragma once

#include "board_ui.hpp"
#include "net.hpp"

namespace pboard::whiteboard {

struct Whiteboard {
  net::ClientState client;
  board_ui::UIState ui;

  explicit Whiteboard(net::ClientId id) : client{id, 0}, ui{client} {}

  void update();
  void render();
};

}; // namespace pboard::whiteboard
