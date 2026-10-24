#pragma once

#include "shape.hpp"

namespace pboard::tool {

enum class Tool { PAN, LINE, RECT, CIRC, SELECT };

struct ToolState {
  Tool selection{Tool::PAN};
  float line_thickness{10.0f};
  shape::Color color{0, 0, 0};
  float x0{0}, y0{0};
};

}; // namespace pboard::tool
