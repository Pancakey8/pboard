#include "menu.hpp"
#include "widget.hpp"
#include <charconv>
#include <cstdint>
#include <print>
#include <raylib.h>
#include <system_error>

namespace pboard::menu {

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

  peer_addr.update(ui_ctx);
  peer_port.update(ui_ctx);
  self_port.update(ui_ctx);
  connect.update();

  if (connect.is_pressed) {
    try_connect(sock, initial_peer, io_context);
  }
}

void MenuState::render() const {
  ClearBackground(Color(255, 255, 255, 255));

  if (!sock_fail.text.empty()) {
    fail_bg.render();
    sock_fail.render(ui_ctx);
  }
  paddr_label.render(ui_ctx);
  peer_addr.render(ui_ctx);
  pport_label.render(ui_ctx);
  peer_port.render(ui_ctx);
  form_div.render();
  sport_label.render(ui_ctx);
  self_port.render(ui_ctx);
  connect.render(ui_ctx);
}

void MenuState::layout() {
  float w = GetRenderWidth();

  float form_width = std::max(150.0f, w / 3.0f);
  float elem_height = 40.0f;
  float gap = 15.0f;

  float x = std::max(0.0f, (w - form_width) / 2.0f);
  float y = 20.0f;

  if (!sock_fail.text.empty()) {
    fail_bg.x = x;
    fail_bg.y = y;
    fail_bg.w = form_width;
    fail_bg.h = elem_height;

    sock_fail.x = x + 4.0f;
    sock_fail.w = form_width;
    sock_fail.h = elem_height;

    auto font = widget::font_of(sock_fail.scale, ui_ctx);
    auto char_size = MeasureTextEx(font, "a", font.baseSize, 0);

    sock_fail.y = y + std::max(0.0f, (elem_height - char_size.y) / 2.0f);

    y += elem_height + gap;
  }

  [&]<typename... Widgets>(Widgets &...w) {
    ((w.x = x, w.w = form_width, w.h = elem_height), ...);
  }(paddr_label, peer_addr, pport_label, peer_port, form_div, sport_label,
    self_port, connect);

  form_div.h = 2.0f;

  {
    auto font = widget::font_of(paddr_label.scale, ui_ctx);
    auto char_size = MeasureTextEx(font, "a", font.baseSize, 0);
    paddr_label.h = pport_label.h = sport_label.h = char_size.y - 6.0f;
  }

  [&]<typename... Widgets>(Widgets &...w) {
    ((w.y = y, y += w.h + gap), ...);
  }(paddr_label, peer_addr, pport_label, peer_port, form_div, sport_label,
    self_port, connect);
}

} // namespace pboard::menu
