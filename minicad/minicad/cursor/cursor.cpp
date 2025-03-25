#include <liberay/math/vec_fwd.hpp>
#include <liberay/util/logger.hpp>
#include <minicad/cursor/cursor.hpp>

namespace minicad {

void Cursor::update(const Camera& camera, eray::math::Vec2f mouse_pos_ndc) {
  if (is_mouse_move_active_) {
    auto old_pos  = transform.pos();
    auto pos_view = camera.view_matrix() * eray::math::Vec4f(old_pos.x, old_pos.y, old_pos.z, 1.F);
    float depth   = pos_view.z;
    auto pos_ndc  = camera.proj_matrix() * pos_view;
    pos_ndc /= pos_ndc.w;

    auto mouse_pos_view =
        camera.inverse_proj_matrix() * eray::math::Vec4f(mouse_pos_ndc.x, mouse_pos_ndc.y, pos_ndc.z, 1.F);
    mouse_pos_view /= mouse_pos_view.w;
    mouse_pos_view.z     = depth;
    auto mouse_pos_world = camera.inverse_view_matrix() * mouse_pos_view;

    transform.set_local_pos(eray::math::Vec3f(mouse_pos_world));
  }
}

}  // namespace minicad
