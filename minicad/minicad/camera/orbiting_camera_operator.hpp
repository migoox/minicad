#pragma once

#include <liberay/math/quat_fwd.hpp>
#include <liberay/math/vec.hpp>
#include <minicad/camera/camera.hpp>

namespace mini {

class OrbitingCameraOperator {
 public:
  OrbitingCameraOperator();

  void start_rot(eray::math::Vec2f mouse_pos);
  void stop_rot();
  bool is_rotating() const { return is_rot_active_; }

  void start_pan(eray::math::Vec2f mouse_pos);
  void stop_pan();
  bool is_panning() const { return is_pan_active_; }

  void start_looking_around(eray::math::Vec2f mouse_pos);
  void stop_looking_around(Camera& camera, eray::math::Transform3f& camera_gimbal);
  bool is_looking_around() const { return is_looking_around_; }

  bool zoom(Camera& camera, float offset);
  bool update(Camera& camera, eray::math::Transform3f& camera_gimbal, eray::math::Vec2f mouse_pos, float dt);

  void set_sensitivity(float sensitivity) { looking_around_sensitivity_ = sensitivity; }

 private:
  eray::math::Vec2f last_mouse_pos_;

  bool is_rot_active_;
  bool is_pan_active_;
  bool is_looking_around_;

  float looking_around_sensitivity_;
  float orbit_sensitivity_;
  float pan_sensitivity_;
  float zoom_sensitivity_;
};

}  // namespace mini
