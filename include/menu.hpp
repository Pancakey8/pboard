#pragma once

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

struct MenuState {
  float w, h;
  InputBox self_port, peer_addr, peer_port;
  Button connect;

  MenuState() {
    self_port.hint = "Listen on...";
    peer_addr.hint = "Peer IP...";
    peer_port.hint = "Peer Port...";
    connect.text = "Connect";
    layout();
  }

  void layout();
  void update(int &sock);
  void render() const;
};

} // namespace pboard::menu
