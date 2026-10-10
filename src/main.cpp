#include "net.hpp"
#include "ui.hpp"
#include <raylib.h>

int main() {
  InitWindow(800, 600, "PBoard");

  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(60);

  pboard::net::ClientState client{0, 0};
  pboard::ui::UIState ui{client};

  while (!WindowShouldClose()) {
    ui.update();

    BeginDrawing();
    ui.render();
    EndDrawing();
  }
}
