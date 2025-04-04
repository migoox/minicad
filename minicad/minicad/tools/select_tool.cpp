#include <minicad/tools/select_tool.hpp>

namespace mini {

namespace math = eray::math;

SelectTool::Rect SelectTool::box(eray::math::Vec2f mouse_pos) {
  auto box_size = math::abs(math::Vec2i(mouse_pos) - math::Vec2i(last_mouse_pos_)) + 1;
  auto start    = math::Vec2i(static_cast<int>(std::min(mouse_pos.x, last_mouse_pos_.x)),
                              static_cast<int>(std::min(mouse_pos.y, last_mouse_pos_.y)));
  return {.pos = start, .size = box_size};
}

}  // namespace mini
