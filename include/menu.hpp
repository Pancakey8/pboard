#pragma once

#include "widget.hpp"
#include <asio.hpp>
#include <raylib.h>
#include <string>

namespace pboard::menu {

struct MenuState {
  const widget::UIContext &ui_ctx;

  float w, h;
  widget::InputBox peer_addr{}, peer_port{}, self_port{};
  widget::Button connect{};
  widget::Label sock_fail{}, paddr_label{}, pport_label{}, sport_label{};
  widget::Rect fail_bg{}, form_div{};

  MenuState(const widget::UIContext &ui_ctx) : ui_ctx(ui_ctx) {
    peer_addr.hint = "e.g. 0.0.0.0";
    paddr_label.text = "Peer address (optional)";
    peer_port.hint = "e.g. 4321";
    pport_label.text = "Peer port (optional)";
    self_port.hint = "e.g. 4321";
    sport_label.text = "Listen port (optional)";
    connect.text = "Connect";

    fail_bg.col = Color(254, 240, 240, 255);
    sock_fail.col = Color(127, 30, 30, 255);
    form_div.col = Color(240, 240, 240, 255);
    paddr_label.col = pport_label.col = sport_label.col =
        Color(70, 70, 70, 255);
    paddr_label.scale = pport_label.scale = sport_label.scale =
        widget::FontScale::TINY;
    layout();
  }

  void layout();
  void update(std::optional<asio::ip::udp::socket> &sock,
              std::optional<asio::ip::udp::endpoint> &initial_peer,
              asio::io_context &io_context);
  void render() const;

  void try_connect(std::optional<asio::ip::udp::socket> &sock,
                   std::optional<asio::ip::udp::endpoint> &initial_peer,
                   asio::io_context &io_context);
};

} // namespace pboard::menu
