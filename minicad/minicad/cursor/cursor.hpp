#pragma once

#include <liberay/math/transform3.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <minicad/camera/camera.hpp>

namespace minicad {

class Cursor {
 public:
  void update(const Camera& camera, eray::math::Vec2f mouse_pos_ndc);

  void start_movement() { is_mouse_move_active_ = true; }

  void stop_movement() { is_mouse_move_active_ = false; }

 public:
  eray::math::Transform3f transform;

 private:
  bool is_mouse_move_active_;
  float start_depth_view_;
  float start_depth_ndc_;
};

}  // namespace minicad
