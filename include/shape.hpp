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

struct Bezier {
  float x0, y0;
  float x1, y1;
  float x2, y2;
  float x3, y3;
  float thick;
  Color col;
};

using Shape = std::variant<Line, Rect, Circ, Bezier>;
  enum class ShapeId : std::uint8_t { LINE, RECT, CIRC, BEZIER };

} // namespace pboard::shape
