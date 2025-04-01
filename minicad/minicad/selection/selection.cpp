#include <liberay/math/vec_fwd.hpp>
#include <libminicad/scene/scene.hpp>
#include <minicad/selection/selection.hpp>

namespace mini {

void Selection::update_centroid(Scene& scene) {
  centroid_ = eray::math::Vec3f::filled(0.F);
  if (is_empty()) {
    return;
  }

  int count = 0;
  for (const auto& obj : objs_) {
    if (auto o = scene.get_obj(obj)) {
      centroid_ += o.value()->transform.pos();
      count++;
    }
  }
  centroid_ /= static_cast<float>(count);
}

}  // namespace mini
