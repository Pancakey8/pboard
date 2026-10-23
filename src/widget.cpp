#include "widget.hpp"
#include "protocol.hpp"
#include "words.hpp"
#include <print>
#include <ranges>
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

std::string name_of_id(net::ClientId id) {
  const auto &b = id.as_bytes();
  std::uint64_t left{}, right{};
  std::memcpy(&left, b.data(), 8);
  std::memcpy(&right, b.data() + 8, 8);

  auto pred = words::PREDICATE_DICT[left % words::PREDICATE_DICT.size()];
  auto obj = words::OBJECT_DICT[right % words::OBJECT_DICT.size()];

  return std::format("{}-{}", pred, obj);
}

void PeerList::update(const net::ClientState &client) {
  for (const auto &[id, ep] : client.peers) {
    if (auto it = client.peer_versions.find(id);
        it != client.peer_versions.end() && it->second.last.expired()) {
      if (auto it = endpoints.find(id); it != endpoints.end()) {
        endpoints.erase(it);
      }
      if (auto it = id_reprs.find(id); it != id_reprs.end()) {
        id_reprs.erase(it);
      }
      continue;
    }

    if (endpoints.contains(id))
      continue;

    endpoints[id] = std::format("{}:{}", ep.address().to_string(), ep.port());
    id_reprs[id] = name_of_id(id);
  }

  if (title_msg.empty()) {
    auto ep = client.sock.local_endpoint();
    title_msg =
        std::format("Peers ({}:{})", ep.address().to_string(), ep.port());
  }

  auto mouse = GetMousePosition();
  float view_h = h + dy / 2;
  float view_y = y - dy / 2;
  bool is_hovered = (x <= mouse.x && mouse.x <= x + w) &&
                    (view_y <= mouse.y && mouse.y <= view_y + view_h);

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    is_active = is_hovered;
  }

  if (is_hovered && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    is_active = true;
    auto delta = GetMouseDelta();
    dy -= delta.y * 2;
  } else if (is_active && GetMouseWheelMove() != 0) {
    auto ds = GetMouseWheelMove() > 0 ? 1 : -1;
    start -= ds;
  }

  start = std::clamp(start, 0, static_cast<int>(endpoints.size()));
  dy = std::max(dy, 2.0f * (TITLEBAR - h));
}

void PeerList::render(const UIContext &ctx) const {
  float view_h = h + dy / 2;
  float view_y = y - dy / 2;

  DrawRectangle(x - BORDER, view_y - BORDER, w + 2 * BORDER,
                view_h + 2 * BORDER, Color(230, 230, 230, 255));

  Color bg = is_active ? Color(255, 255, 255, 255) : Color(240, 240, 240, 255);
  DrawRectangle(x, view_y, w, view_h, bg);

  auto font = font_of(FontScale::MED, ctx);
  auto char_size = MeasureTextEx(font, "a", font.baseSize, 0);

  Color title =
      is_active ? Color(220, 220, 220, 255) : Color(200, 200, 200, 255);

  DrawRectangle(x, view_y, w, TITLEBAR, title);
  DrawTextEx(font, title_msg.c_str(),
             {x + PADDING, view_y + (TITLEBAR - char_size.y) / 2.0f},
             font.baseSize, 0, Color(20, 20, 20, 255));

  std::size_t line{1 + static_cast<std::size_t>(start)};

  float my{view_y + TITLEBAR + PADDING};
  float hdiv{2.0f};

  bool first{true};
  for (const auto &[id, endpoint] : endpoints | std::views::drop(start)) {
    if (my + char_size.y * 2 + PADDING >= view_y + view_h)
      break;

    if (!first) {
      my += PADDING / 2;
      DrawRectangle(x + PADDING, my, w - 2 * PADDING, hdiv,
                    Color(200, 200, 200, 255));
      my += hdiv;
    } else {
      first = false;
    }

    my += PADDING / 2;

    auto name_it = id_reprs.find(id);
    const char *name =
        name_it == id_reprs.end() ? "N/A" : name_it->second.c_str();

    std::string tex{std::to_string(line) + ". "};
    DrawTextEx(font, tex.c_str(), {x + PADDING, my}, font.baseSize, 0,
               Color(20, 20, 20, 255));

    DrawTextEx(font, name, {x + PADDING + char_size.x * tex.size(), my},
               font.baseSize, 0, Color(20, 20, 20, 255));
    my += char_size.y;
    DrawTextEx(font, endpoint.c_str(), {x + PADDING, my}, font.baseSize, 0,
               Color(20, 20, 20, 255));
    my += char_size.y + PADDING / 2;

    line++;
  }
}

} // namespace pboard::widget
