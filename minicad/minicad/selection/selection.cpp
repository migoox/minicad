#include <liberay/math/vec_fwd.hpp>
#include <libminicad/scene/scene.hpp>
#include <minicad/selection/selection.hpp>

namespace mini {

using Vec3f = eray::math::Vec3f;

void Selection::remove(Scene& scene, const SceneObjectHandle& handle) {
  detach_all(scene);

  objs_.erase(objs_.find(handle));
  update_centroid(scene);
}

void Selection::add(Scene& scene, const SceneObjectHandle& handle) {
  detach_all(scene);

  objs_.insert(handle);
  update_centroid(scene);
}

void Selection::clear(Scene& scene) {
  detach_all(scene);

  objs_.clear();
  transform.reset_local(eray::math::Vec3f::filled(0.F));
}

void Selection::mark_transform_dirty(Scene& scene) {
  for (const auto& handle : objs_) {
    if (auto o = scene.get_obj(handle)) {
      o.value()->mark_dirty();
    }
  }

  if (transform_dirty_) {
    return;
  }

  transform_dirty_ = true;
  for (const auto& handle : objs_) {
    if (auto o = scene.get_obj(handle)) {
      o.value()->transform.set_parent(transform);
    }
  }
}

void Selection::detach_all(Scene& scene) {
  if (!transform_dirty_) {
    return;
  }
  transform_dirty_ = false;

  for (const auto& handle : objs_) {
    if (auto o = scene.get_obj(handle)) {
      o.value()->transform.detach_from_parent();
    }
  }
}

void Selection::update_centroid(Scene& scene) {
  auto centroid = eray::math::Vec3f::filled(0.F);
  if (is_empty()) {
    transform.reset_local(centroid);
    return;
  }

  int count = 0;
  for (const auto& obj : objs_) {
    if (auto o = scene.get_obj(obj)) {
      centroid += o.value()->transform.pos();
      count++;
    }
  }
  centroid /= static_cast<float>(count);
  transform.reset_local(centroid);
}

}  // namespace mini
