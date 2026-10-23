#include "menu.hpp"
#include "space-mono.h"
#include "whiteboard.hpp"
#include "widget.hpp"
#include <raylib.h>

int main() {
  InitWindow(800, 600, "PBoard");

  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(60);

  Font space_mono_big =
      LoadFontFromMemory(".ttf", space_mono, space_mono_len, 24, NULL, 0);

  Font space_mono_med =
      LoadFontFromMemory(".ttf", space_mono, space_mono_len, 20, NULL, 0);

  Font space_mono_tiny =
      LoadFontFromMemory(".ttf", space_mono, space_mono_len, 16, NULL, 0);

  pboard::widget::UIContext ui_ctx{space_mono_big, space_mono_med,
                                   space_mono_tiny};

  asio::io_context io_context;

  std::optional<asio::ip::udp::socket> sock{};
  std::optional<asio::ip::udp::endpoint> initial_peer{};

  {
    pboard::menu::MenuState menu{ui_ctx};

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

  if (!WindowShouldClose()) {
    pboard::whiteboard::Whiteboard board{ui_ctx, io_context,
                                         std::move(sock.value()), initial_peer};

    HideCursor();

    while (!WindowShouldClose()) {
      board.update();

      BeginDrawing();
      board.render();
      EndDrawing();
    }
  }

  UnloadFont(space_mono_tiny);
  UnloadFont(space_mono_med);
  UnloadFont(space_mono_big);

  CloseWindow();
}
