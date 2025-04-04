#pragma once
#include <liberay/math/vec.hpp>

namespace mini {

class SelectTool {
 public:
  struct Rect {
    eray::math::Vec2i pos;
    eray::math::Vec2i size;
  };

  void start_box_select(eray::math::Vec2f mouse_pos) {
    box_select_active_ = true;
    last_mouse_pos_    = mouse_pos;
  }

  void end_box_select() { box_select_active_ = false; }

  Rect box(eray::math::Vec2f mouse_pos);

  bool is_box_select_active() const { return box_select_active_; }

 private:
  eray::math::Vec2f last_mouse_pos_;
  bool box_select_active_{};
};

}  // namespace mini
