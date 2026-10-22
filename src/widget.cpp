#include "widget.hpp"
#include <print>
#include <raylib.h>

namespace pboard::widget {
Font font_of(FontScale scale, const UIContext &ctx) {
  switch (scale) {
  case FontScale::TINY:
    return ctx.font_tiny;
  case FontScale::MED:
    return ctx.font_med;
  case FontScale::BIG:
    return ctx.font_big;
  }

  std::println("LOG: Bad font scale");
  return GetFontDefault();
}

void InputBox::update(const UIContext &ctx) {
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    auto m = GetMousePosition();
    is_active = (x <= m.x && m.x <= x + w) && (y <= m.y && m.y <= y + h);
  }

  if (!is_active) {
    return;
  }

  auto font = font_of(scale, ctx);
  auto char_size = MeasureTextEx(font, "a", font.baseSize, 0);
  int max_visible =
      std::max(1, static_cast<int>((w - 2 * PADDING) / char_size.x));

  std::size_t cursor_orig{cursor};

  if (IsKeyPressed(KEY_LEFT) && cursor > 0) {
    cursor--;
  } else if (IsKeyPressed(KEY_RIGHT) && cursor < content.size()) {
    cursor++;
  } else if (IsKeyPressed(KEY_BACKSPACE) && cursor > 0) {
    cursor--;
    content.erase(content.begin() + cursor);
  }

  while (true) {
    int key = GetCharPressed();
    if (key <= 0)
      break;

    if (32 <= key && key <= 125) {
      content.insert(content.begin() + cursor, static_cast<char>(key));
      cursor++;
    }
  }

  if (cursor != cursor_orig) {
    if (cursor < left) {
      left = cursor;
    } else if (cursor > left + max_visible) {
      left = cursor - max_visible;
    }

    if (content.size() <= static_cast<std::size_t>(max_visible)) {
      left = 0;
    } else if (left > content.size() - max_visible) {
      left = content.size() - max_visible;
    }

    auto view_len =
        std::min(static_cast<std::size_t>(max_visible), content.size() - left);
    view = content.substr(left, view_len);
  }
}

void InputBox::render(const UIContext &ctx) const {
  auto font = font_of(scale, ctx);
  auto char_size = MeasureTextEx(font, "a", font.baseSize, 0);

  DrawRectangle(x - BORDER, y - BORDER, w + 2 * BORDER, h + 2 * BORDER,
                Color(230, 230, 230, 255));

  Color bg = is_active ? Color(255, 255, 255, 255) : Color(240, 240, 240, 255);
  DrawRectangle(x, y, w, h, bg);

  const char *text = !content.empty() ? view.c_str() : hint.c_str();
  Color col =
      !content.empty() ? Color(20, 20, 20, 255) : Color(180, 180, 180, 255);

  float text_y =
      y + PADDING + std::max(0.0f, (h - 2 * PADDING - char_size.y) / 2.0f);

  DrawTextEx(font, text, {x + PADDING, text_y}, font.baseSize, 0, col);

  if (is_active) {
    auto dist = (cursor - left) * char_size.x;
    DrawRectangle(x + PADDING + dist, text_y, 2, char_size.y,
                  Color(20, 20, 20, 255));
  }
}

void Button::update() {
  auto m = GetMousePosition();

  is_hovered = (x <= m.x && m.x <= x + w) && (y <= m.y && m.y <= y + h);

  is_pressed = (is_hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
}

void Button::render(const UIContext &ctx) const {
  DrawRectangle(x - BORDER, y - BORDER, w + 2 * BORDER, h + 2 * BORDER,
                Color(15, 20, 45, 255));
  auto bg = is_hovered ? Color(30, 40, 60, 255) : Color(15, 20, 45, 255);
  DrawRectangle(x, y, w, h, bg);

  auto font = font_of(scale, ctx);
  auto char_size = MeasureTextEx(font, "a", font.baseSize, 0);

  DrawTextEx(
      font, text.c_str(),
      {x + PADDING,
       y + PADDING + std::max(0.0f, (h - 2 * PADDING - char_size.y) / 2.0f)},
      font.baseSize, 0, Color(255, 255, 255, 255));
}

void Rect::render() const { DrawRectangle(x, y, w, h, col); }

void Label::render(const UIContext &ctx) const {
  auto font = font_of(scale, ctx);
  auto char_size = MeasureTextEx(font, "a", font.baseSize, 0);

  std::size_t max_visible = std::max(1, static_cast<int>(w / char_size.x)) - 1;

  std::string_view sv{text.begin(),
                      text.begin() + std::min(max_visible, text.size())};

  if (sv != visible_cache) {
    visible_cache = sv;
  }

  DrawTextEx(font, visible_cache.c_str(), {x, y}, font.baseSize, 0, col);
}

} // namespace pboard::widget
