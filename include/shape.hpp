#pragma once

#include <cstdint>
#include <variant>

namespace pboard::shape {

struct Color {
  float r, g, b;
};

struct Line {
  float x0, y0, x1, y1;
  float thick;
  Color col;
};

struct Rect {
  float x, y, w, h;
  Color col;
};

struct Circ {
  float x, y, r;
  Color col;
};

using Shape = std::variant<Line, Rect, Circ>;
enum class ShapeId : std::uint8_t { LINE, RECT, CIRC };

} // namespace pboard::shape
