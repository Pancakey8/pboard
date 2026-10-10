#include "menu.hpp"
#include <print>
#include <raylib.h>

namespace pboard::menu {

void InputBox::update() {
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    auto m = GetMousePosition();
    is_active = (x <= m.x && m.x <= x + w) && (y <= m.y && m.y <= y + h);
  }

  if (!is_active) {
    return;
  }

  if (MeasureText(content.c_str(), 16) <= w - 24) {
    int key = GetCharPressed();
    while (key > 0) {
      if ((key >= 32) && (key <= 125)) {
        content.insert(content.begin() + cursor, static_cast<char>(key));
        cursor++;
      }

      key = GetCharPressed();
    }
  }

  if (IsKeyPressed(KEY_LEFT) && cursor > 0) {
    cursor--;
  }

  if (IsKeyPressed(KEY_RIGHT) && cursor < content.size()) {
    cursor++;
  }

  if (IsKeyPressed(KEY_BACKSPACE) && cursor > 0) {
    content.erase(content.begin() + cursor - 1);
    cursor--;
  }
}

void InputBox::render() const {
  DrawRectangle(x, y, w, h, WHITE);
  if (!content.empty()) {
    DrawText(content.c_str(), x + 4, y + (h - 16) / 2, 16, BLACK);
  } else {
    DrawText(hint.c_str(), x + 4, y + (h - 16) / 2, 16,
             Color(220, 220, 220, 255));
  }

  if (is_active) {
    std::string sub = content.substr(0, cursor);
    int dist = MeasureText(sub.c_str(), 16);
    DrawRectangle(x + 4 + dist, y + (h - 16) / 2, 2, 16, GRAY);
  }
}

void Button::update() {
  auto m = GetMousePosition();
  is_hovered = ((x <= m.x && m.x <= x + w) && (y <= m.y && m.y <= y + h));
  is_pressed = IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && is_hovered;
}

void Button::render() const {
  DrawRectangle(x, y, w, h, BLACK);
  int size = MeasureText(text.c_str(), 16);

  DrawText(text.c_str(), x + (w - size) / 2, y + (h - 16) / 2, 16, WHITE);
}

void MenuState::update(int &sock) {
  if (IsWindowResized()) {
    layout();
  }

  peer_addr.update();
  peer_port.update();
  self_port.update();
  connect.update();

  if (connect.is_pressed) {
    std::println("Listen on 0.0.0.0:{}", self_port.content);
    std::println("Connect to {}:{}", peer_addr.content, peer_port.content);
    sock = 1234; // TODO
  }
}

void MenuState::render() const {
  ClearBackground(Color(180, 180, 180, 255));
  peer_addr.render();
  peer_port.render();
  self_port.render();
  connect.render();
}

void MenuState::layout() {
  float w_form = 180;
  float h_each = 40;
  float gap = 20;

  float min_w = w_form + 2 * gap;
  float min_h = h_each * 4 + gap * 3 + 2 * gap;

  float w = std::max(min_w, static_cast<float>(GetRenderWidth()));
  float h = std::max(min_h, static_cast<float>(GetRenderHeight()));

  connect.w = self_port.w = peer_addr.w = peer_port.w = w_form;
  connect.h = self_port.h = peer_addr.h = peer_port.h = h_each;
  connect.x = self_port.x = peer_addr.x = peer_port.x = gap + (w - min_w) / 2;

  float y = gap + (h - min_h) / 2;

  peer_addr.y = y;
  y += h_each + gap;

  peer_port.y = y;
  y += h_each + gap;

  self_port.y = y;
  y += h_each + gap;

  connect.y = y;
  y += h_each + gap;
}

} // namespace pboard::menu
