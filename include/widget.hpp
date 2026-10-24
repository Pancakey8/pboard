#pragma once

#include "net.hpp"
#include "protocol.hpp"
#include "tool.hpp"
#include <raylib.h>
#include <string>

namespace pboard::widget {
struct UIContext {
  Font font_big;
  Font font_med;
  Font font_tiny;
};

enum class FontScale { TINY, MED, BIG };

Font font_of(FontScale scale, const UIContext &ctx);

struct InputBox {
  float x, y, w, h;
  FontScale scale{FontScale::MED};

  std::string hint{};
  std::string content{};

  std::size_t cursor{0};
  std::size_t left{0};
  std::string view{};
  bool is_active{false};

  static constexpr float PADDING{4.0f};
  static constexpr float BORDER{2.0f};

  void update(const UIContext &ctx);
  void render(const UIContext &ctx) const;
};

struct Button {
  float x, y, w, h;
  FontScale scale{FontScale::MED};
  std::string text{};

  bool is_pressed{false};
  bool is_hovered{false};

  static constexpr float PADDING{4.0f};
  static constexpr float BORDER{2.0f};

  void update();
  void render(const UIContext &ctx) const;
};

struct Rect {
  float x, y, w, h;
  Color col;

  void render() const;
};

struct Label {
  float x, y, w, h;
  FontScale scale{FontScale::MED};
  std::string text{};
  Color col;

  mutable std::string visible_cache{};

  void render(const UIContext &ctx) const;
};

struct PeerList {
  float x, y, w, h;

  float dy{};
  int start{};
  bool is_active{false};

  std::flat_map<net::ClientId, std::string> endpoints{};
  std::flat_map<net::ClientId, std::string> id_reprs{};

  std::string title_msg{};

  static constexpr float TITLEBAR{40.0f};

  static constexpr float PADDING{4.0f};
  static constexpr float BORDER{2.0f};

  void update(const net::ClientState &client);
  void render(const UIContext &ctx) const;
};

struct ColorButton {
  float x, y, r;
  Color color;

  bool is_pressed{false};
  bool is_hovered{false};

  static constexpr float BORDER{2.0f};

  void update();
  void render() const;
};
  
struct ToolMenu {
  float x, y, w, h;

  static constexpr float PADDING{4.0f};
  static constexpr float BORDER{2.0f};

  Button pan{}, select{}, rect{}, line{}, circ{};

  std::array<Color, 8> hotbar{};
  std::array<ColorButton, 8> colors{};

  bool is_active{false};

  ToolMenu() {
    pan.text = "Pan";
    select.text = "Select";
    rect.text = "Rectangle";
    line.text = "Line";
    circ.text = "Circle";

    for (auto &col : colors) {
      col.color = Color(255, 0, 0, 255);
    }

    hotbar[0] = GetColor(0x1E293BFF);
    hotbar[1] = GetColor(0x64748BFF);
    hotbar[2] = GetColor(0x2563EBFF);
    hotbar[3] = GetColor(0x16A34AFF);
    hotbar[4] = GetColor(0xDC2626FF);
    hotbar[5] = GetColor(0xD97706FF);
    hotbar[6] = GetColor(0x9333EAFF);
    hotbar[7] = GetColor(0x0891B2FF);
  }

  void update(tool::ToolState &tool);
  void render(const tool::ToolState &tool, const UIContext &ctx) const;
};

} // namespace pboard::widget
