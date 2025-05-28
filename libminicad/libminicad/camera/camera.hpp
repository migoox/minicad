#pragma once

#include <liberay/math/transform3.hpp>
#include <liberay/util/ruleof.hpp>

namespace mini {

class Camera {
 public:
  eray::math::Transform3f transform;

  Camera(bool orthographic, float fov, float aspect_ratio, float near_plane, float far_plane);

  bool is_orthographic() const { return is_orthographic_; }
  float fov() const { return fov_; }
  float aspect_ratio() const { return aspect_ratio_; }
  float near_plane() const { return near_plane_; }
  float far_plane() const { return far_plane_; }
  float width() const { return width_; }
  float height() const { return height_; }
  float stereo_convergence_distance() const { return stereo_convergence_distance_; }

  void set_orthographic(bool ortho);
  void set_fov(float fov);
  void set_aspect_ratio(float aspect_ratio);
  void set_near_plane(float near_plane);
  void set_far_plane(float far_plane);
  void set_stereo_convergence_distance(float stereo_convergence_distance) {
    stereo_convergence_distance_ = stereo_convergence_distance;
  }

  const eray::math::Mat4f& view_matrix() const { return transform.world_to_local_matrix(); }
  const eray::math::Mat4f& inverse_view_matrix() const { return transform.local_to_world_matrix(); }
  const eray::math::Mat4f& proj_matrix() const { return projection_; }
  const eray::math::Mat4f& inverse_proj_matrix() const { return projection_inv_; }

  eray::math::Mat4f stereo_left_proj_matrix() const;
  eray::math::Mat4f stereo_right_proj_matrix() const;

  void recalculate_projection();

 private:
  bool is_orthographic_;

  float fov_;  // only apparent if orthographic
  float aspect_ratio_;
  float width_, height_;
  float near_plane_, far_plane_;

  float stereo_convergence_distance_;

  eray::math::Mat4f projection_, projection_inv_;
};

}  // namespace mini
