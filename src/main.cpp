#include "menu.hpp"
#include "whiteboard.hpp"
#include <raylib.h>

int main() {
  InitWindow(800, 600, "PBoard");

  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(60);

  asio::io_context io_context;

  std::optional<asio::ip::udp::socket> sock;
  bool is_host{false};

  {
    pboard::menu::MenuState menu{};

    while (!WindowShouldClose()) {
      menu.update(sock, is_host, io_context);
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
                                         is_host};

    while (!WindowShouldClose()) {
      board.update();

      BeginDrawing();
      board.render();
      EndDrawing();
    }
  }
}
