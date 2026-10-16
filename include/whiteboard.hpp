#pragma once

#include "board_ui.hpp"
#include "net.hpp"
#include <asio.hpp>

namespace pboard::whiteboard {

struct Whiteboard {
  net::ClientState client;
  board_ui::UIState ui;

  explicit Whiteboard(asio::io_context &io_context,
                      asio::ip::udp::socket &&sock, std::optional<asio::ip::udp::endpoint> initial_peer)
      : client{io_context, std::move(sock), initial_peer}, ui{client} {
    client.intro();
  }

  void update();
  void render();
};

}; // namespace pboard::whiteboard
