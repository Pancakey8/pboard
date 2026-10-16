#pragma once

#include <asio.hpp>
#include <raylib.h>
#include <string>

namespace pboard::menu {

struct InputBox {
  float x, y, w, h;
  std::string hint{};
  std::string content{};
  std::size_t cursor{0};
  bool is_active{false};

  void update();
  void render() const;
};

struct Button {
  float x, y, w, h;
  bool is_pressed{false};
  bool is_hovered{false};
  std::string text;

  void update();
  void render() const;
};

struct Label {
  float x, y, w, h;
  std::string text;
  Color col;
  void render() const;
};

struct MenuState {
  float w, h;
  InputBox self_port, peer_addr, peer_port;
  Button connect;
  Label form_div;
  Label sock_fail;

  MenuState() {
    self_port.hint = "Listen on...";
    peer_addr.hint = "Peer IP...";
    peer_port.hint = "Peer Port...";
    connect.text = "Connect";
    form_div.text = "or";
    form_div.col = BLACK;
    sock_fail.text = "";
    sock_fail.col = RED;
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
