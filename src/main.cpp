#include "menu.hpp"
#include "whiteboard.hpp"
#include <raylib.h>

int main() {
  InitWindow(800, 600, "PBoard");

  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(60);

  int sd{-1};

  {
    pboard::menu::MenuState menu{};

    while (!WindowShouldClose()) {
      menu.update(sd);
      if (sd != -1) {
        break;
      }

      BeginDrawing();
      menu.render();
      EndDrawing();
    }
  }

  {
    pboard::whiteboard::Whiteboard board{0};

    while (!WindowShouldClose()) {
      board.update();

      BeginDrawing();
      board.render();
      EndDrawing();
    }
  }
}
