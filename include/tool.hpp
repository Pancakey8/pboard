#pragma once

#include "shape.hpp"
#include <array>

namespace pboard::tool {

enum class Tool { PAN, LINE, RECT, CIRC, SELECT, BEZIER };

struct BezierPoints {
  std::array<float, 8> pts{};
  std::uint64_t idx{0};
};

struct ToolState {
  Tool selection{Tool::PAN};
  float line_thickness{10.0f};
  shape::Color color{0, 0, 0};
  float x0{0}, y0{0};
  BezierPoints bez;
};

}; // namespace pboard::tool
