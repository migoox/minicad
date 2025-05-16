#include <liberay/math/mat.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/util/logger.hpp>
#include <minicad/camera/orbiting_camera_operator.hpp>

namespace mini {

OrbitingCameraOperator::OrbitingCameraOperator()
    : last_mouse_pos_(eray::math::Vec2f::filled(0.0F)),
      is_rot_active_(false),
      is_pan_active_(false),
      looking_around_sensitivity_(0.1F),
      orbit_sensitivity_(0.32F),
      pan_sensitivity_(0.016F),
      zoom_sensitivity_(0.4F) {}

void OrbitingCameraOperator::start_rot(eray::math::Vec2f mouse_pos) {
  is_rot_active_  = true;
  last_mouse_pos_ = mouse_pos;
}

void OrbitingCameraOperator::stop_rot() { is_rot_active_ = false; }

void OrbitingCameraOperator::start_pan(eray::math::Vec2f mouse_pos) {
  is_pan_active_  = true;
  last_mouse_pos_ = mouse_pos;
}

void OrbitingCameraOperator::stop_pan() { is_pan_active_ = false; }

bool OrbitingCameraOperator::zoom(Camera& camera, float offset) {  // NOLINT
  camera.transform.move_local(eray::math::Vec3f(0.F, 0.F, zoom_sensitivity_ * offset));
  return true;
}

void OrbitingCameraOperator::start_looking_around(eray::math::Vec2f mouse_pos) {
  is_looking_around_ = true;
  last_mouse_pos_    = mouse_pos;
}

void OrbitingCameraOperator::stop_looking_around(Camera& /*camera*/, eray::math::Transform3f& /*camera_gimbal*/) {
  //   auto cam_glob_pos = camera.transform.pos();
  //   camera_gimbal.set_local_pos(cam_glob_pos - camera.transform.local_front() *
  //   camera.transform.local_pos().length());

  //   camera_gimbal.set_local_pos(eray::math::Vec3f(
  //       camera.transform.local_to_parent_matrix() *
  //       eray::math::translation(camera.transform.local_front() * camera.transform.local_pos().length()) *
  //       camera.transform.parent_to_local_matrix() *
  //       eray::math::Vec4f(camera_gimbal.pos().x, camera_gimbal.pos().y, camera_gimbal.pos().z, 1.F)));

  is_looking_around_ = false;
}

bool OrbitingCameraOperator::update(Camera& camera, eray::math::Transform3f& camera_gimbal, eray::math::Vec2f mouse_pos,
                                    float dt) {
  if (is_rot_active_) {
    eray::math::Vec2f mouse_delta = (mouse_pos - last_mouse_pos_) * orbit_sensitivity_;

    auto rot     = camera.transform.local_rot();
    auto yaw     = eray::math::Quatf::rotation_y(-mouse_delta.x * dt);
    auto pitch   = eray::math::Quatf::rotation_x(-mouse_delta.y * dt);
    auto new_rot = eray::math::normalize(yaw * rot * pitch);

    float camera_distance = camera.transform.local_pos().length();

    camera.transform.set_local_rot(new_rot);
    camera.transform.set_local_pos(-camera.transform.front() * camera_distance);
    camera.recalculate_projection();
    last_mouse_pos_ = mouse_pos;
    return true;
  }

  if (is_pan_active_) {
    eray::math::Vec2f mouse_delta = (mouse_pos - last_mouse_pos_) * pan_sensitivity_;

    last_mouse_pos_ = mouse_pos;
    camera_gimbal.move(
        eray::math::Vec3f(camera.inverse_view_matrix() * eray::math::Vec4f(-mouse_delta.x, mouse_delta.y, 0.F, 0.F)));

    // Alternative equivalent approach:
    // camera_gimbal.move(camera.transform.local_right() * mouse_delta.x - camera.transform.local_up() * mouse_delta.y);
    return true;
  }

  if (is_looking_around_) {
    // eray::math::Vec2f mouse_delta = (mouse_pos - last_mouse_pos_) * looking_around_sensitivity_;

    // auto rot     = camera.transform.local_rot();
    // auto yaw     = eray::math::Quatf::rotation_y(-mouse_delta.x * dt);
    // auto pitch   = eray::math::Quatf::rotation_axis(-mouse_delta.y * dt, camera.transform.local_right());
    // auto new_rot = eray::math::normalize(yaw * pitch * rot);
    // camera.transform.set_local_rot(new_rot);

    last_mouse_pos_ = mouse_pos;
    return true;
  }

  return false;
}

}  // namespace mini
