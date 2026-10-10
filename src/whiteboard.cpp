#include "whiteboard.hpp"

namespace pboard::whiteboard {
  void Whiteboard::update() {
    client.update();
    ui.update();
  }

  void Whiteboard::render() {
    ui.render();
  }
}
