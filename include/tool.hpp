#pragma once

#include "net.hpp"
#include "protocol.hpp"
#include "shape.hpp"
#include <array>

namespace pboard::tool {

enum class Tool { PAN, LINE, RECT, CIRC, SELECT, BEZIER };

struct PanTool {};

enum class ToolStatus { NONE, DRAW, FINAL };

struct LineTool {
  ToolStatus status;
  float x0{}, y0{}, x1{}, y1{};
};

struct RectTool {
  ToolStatus status;
  float x0{}, y0{}, x1{}, y1{};
};

struct CircTool {
  ToolStatus status;
  float x0{}, y0{}, x1{}, y1{};
};

struct SelectTool {
  std::optional<net::ObjectId> selection{};
};

struct BezierTool {
  std::array<float, 8> pts{};
  std::uint64_t idx{0};
};

using ToolData =
    std::variant<PanTool, LineTool, RectTool, CircTool, SelectTool, BezierTool>;

struct ToolState {
  Tool selection{Tool::PAN};
  ToolData data{PanTool{}};
  float line_thickness{10.0f};
  shape::Color color{0, 0, 0};

  std::optional<shape::Shape> ghost(float vx, float vy, float scale) const;

  std::optional<shape::Shape> update(net::ClientState &client, float vx,
                                     float vy, float scale);
};

}; // namespace pboard::tool
