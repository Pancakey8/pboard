#pragma once

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
} // namespace pboard::widget
