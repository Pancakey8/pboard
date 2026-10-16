#include "menu.hpp"
#include "whiteboard.hpp"
#include <raylib.h>

int main() {
  InitWindow(800, 600, "PBoard");

  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(60);

  asio::io_context io_context;

  std::optional<asio::ip::udp::socket> sock;
  std::optional<asio::ip::udp::endpoint> initial_peer{};

  {
    pboard::menu::MenuState menu{};

    while (!WindowShouldClose()) {
      menu.update(sock, initial_peer, io_context);
      if (sock.has_value()) {
        break;
      }

      BeginDrawing();
      menu.render();
      EndDrawing();
    }
  }

  {
    pboard::whiteboard::Whiteboard board{io_context, std::move(sock.value()),
                                         initial_peer};

    while (!WindowShouldClose()) {
      board.update();

      BeginDrawing();
      board.render();
      EndDrawing();
    }
  }
}
