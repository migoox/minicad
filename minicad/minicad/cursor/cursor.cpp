#include <liberay/math/vec_fwd.hpp>
#include <liberay/util/logger.hpp>
#include <minicad/cursor/cursor.hpp>

namespace mini {

bool Cursor::update(const Camera& camera, eray::math::Vec2f mouse_pos_ndc) {
  if (is_mouse_move_active_) {
    set_by_ndc_pos(camera, mouse_pos_ndc);
    return true;
  }
  return false;
}

void Cursor::update_ndc_pos(const Camera& camera) {
  auto old_pos  = transform.pos();
  auto ndc_temp = camera.proj_matrix() * camera.view_matrix() * eray::math::Vec4f(old_pos.x, old_pos.y, old_pos.z, 1.F);
  ndc_temp /= ndc_temp.w;
  ndc_pos_ = eray::math::Vec2f(ndc_temp);
}

eray::math::Vec2f Cursor::ndc_pos(const Camera& camera) {
  if (is_ndc_pos_dirty_) {
    update_ndc_pos(camera);
  }

  return ndc_pos_;
}

void Cursor::set_by_ndc_pos(const Camera& camera, eray::math::Vec2f ndc_pos) {
  auto old_pos      = transform.pos();
  auto old_pos_view = camera.view_matrix() * eray::math::Vec4f(old_pos.x, old_pos.y, old_pos.z, 1.F);
  float depth       = old_pos_view.z;
  auto old_ndc_pos  = camera.proj_matrix() * old_pos_view;
  old_ndc_pos /= old_ndc_pos.w;

  auto pos_view = camera.inverse_proj_matrix() * eray::math::Vec4f(ndc_pos.x, ndc_pos.y, old_ndc_pos.z, 1.F);
  pos_view /= pos_view.w;
  pos_view.z     = depth;
  auto pos_world = camera.inverse_view_matrix() * pos_view;

  transform.set_local_pos(eray::math::Vec3f(pos_world));
}

}  // namespace mini
