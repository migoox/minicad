#pragma once

#include <liberay/math/transform3.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <minicad/camera/camera.hpp>

namespace mini {

class Cursor {
 public:
  Cursor() = default;

  bool update(const Camera& camera, eray::math::Vec2f mouse_pos_ndc);
  eray::math::Vec2f ndc_pos(const Camera& camera);
  void set_by_ndc_pos(const Camera& camera, eray::math::Vec2f ndc_pos);

  void mark_dirty() { is_ndc_pos_dirty_ = true; }

  void start_movement() { is_mouse_move_active_ = true; }

  void stop_movement() { is_mouse_move_active_ = false; }

 public:
  eray::math::Transform3f transform;

 private:
  void update_ndc_pos(const Camera& camera);

 private:
  bool is_ndc_pos_dirty_{true};
  eray::math::Vec2f ndc_pos_;

  bool is_mouse_move_active_{};
  float start_depth_view_{};
  float start_depth_ndc_{};
};

}  // namespace mini
