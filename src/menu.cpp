#include "menu.hpp"
#include <charconv>
#include <cstdint>
#include <print>
#include <raylib.h>
#include <system_error>

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

void Label::render() const {
  int size = MeasureText(text.c_str(), 24);
  DrawText(text.c_str(), x + (w - size) / 2, y + (h - 24) / 2, 24, col);
}

void MenuState::try_connect(
    std::optional<asio::ip::udp::socket> &sock,
    std::optional<asio::ip::udp::endpoint> &initial_peer,
    asio::io_context &io_context) {
  bool is_peer = !peer_addr.content.empty() || !peer_port.content.empty();
  bool is_self = !self_port.content.empty();

  try {
    std::uint16_t port = 0;
    if (is_self) {
      auto [ptr, ec] = std::from_chars(
          self_port.content.data(),
          self_port.content.data() + self_port.content.size(), port);
      if (ec != std::errc()) {
        sock_fail.text = "Listen port is invalid.";
        layout();
        return;
      }
    }
    asio::ip::udp::endpoint endpoint(asio::ip::udp::v4(), port);
    sock = asio::ip::udp::socket(io_context, endpoint);
    sock->non_blocking(true);
    std::println("On endpoint {}:{}",
                 sock->local_endpoint().address().to_string(),
                 sock->local_endpoint().port());
  } catch (const std::exception &e) {
    sock_fail.text = "Failed to bind port: " + std::string(e.what());
    layout();
    return;
  }

  if (is_peer) {
    try {
      asio::ip::udp::resolver resolver(io_context);
      auto endpoints = resolver.resolve(asio::ip::udp::v4(), peer_addr.content,
                                        peer_port.content);
      initial_peer = endpoints.begin()->endpoint();
    } catch (const std::exception &e) {
      sock_fail.text = "Failed to resolve peer: " + std::string(e.what());
      layout();
      return;
    }
  } else {
    initial_peer.reset();
  }
}

void MenuState::update(std::optional<asio::ip::udp::socket> &sock,
                       std::optional<asio::ip::udp::endpoint> &initial_peer,
                       asio::io_context &io_context) {
  if (IsWindowResized()) {
    layout();
  }

  peer_addr.update();
  peer_port.update();
  self_port.update();
  connect.update();

  if (connect.is_pressed) {
    try_connect(sock, initial_peer, io_context);
  }
}

void MenuState::render() const {
  ClearBackground(Color(180, 180, 180, 255));
  sock_fail.render();
  peer_addr.render();
  peer_port.render();
  form_div.render();
  self_port.render();
  connect.render();
}

void MenuState::layout() {
  float w_form = 180;
  float h_each = 40;
  float gap = 20;
  int count = sock_fail.text.empty() ? 5 : 6;

  float min_w = w_form + 2 * gap;
  float min_h = h_each * count + gap * (count - 1) + 2 * gap;

  float w = std::max(min_w, static_cast<float>(GetRenderWidth()));
  float h = std::max(min_h, static_cast<float>(GetRenderHeight()));

  connect.w = self_port.w = peer_addr.w = peer_port.w = sock_fail.w =
      form_div.w = w_form;
  connect.h = self_port.h = peer_addr.h = peer_port.h = sock_fail.h =
      form_div.h = h_each;
  connect.x = self_port.x = peer_addr.x = peer_port.x = sock_fail.x =
      form_div.x = gap + (w - min_w) / 2;

  float y = gap + (h - min_h) / 2;

  if (!sock_fail.text.empty()) {
    sock_fail.y = y;
    y += h_each + gap;
  }

  peer_addr.y = y;
  y += h_each + gap;

  peer_port.y = y;
  y += h_each + gap;

  form_div.y = y;
  y += h_each + gap;

  self_port.y = y;
  y += h_each + gap;

  connect.y = y;
  y += h_each + gap;
}

} // namespace pboard::menu
