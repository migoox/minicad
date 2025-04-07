#include <liberay/math/vec_fwd.hpp>
#include <libminicad/scene/scene.hpp>
#include <minicad/selection/selection.hpp>

namespace mini {

using Vec3f = eray::math::Vec3f;

void SceneObjectsSelection::remove(Scene& scene, const SceneObjectHandle& handle) {
  auto f = objs_.find(handle);
  if (f == objs_.end()) {
    return;
  }
  detach_all(scene);

  objs_.erase(objs_.find(handle));
  update_centroid(scene);
}

void SceneObjectsSelection::add(Scene& scene, const SceneObjectHandle& handle) {
  detach_all(scene);

  objs_.insert(handle);
  update_centroid(scene);
}

void SceneObjectsSelection::clear(Scene& scene) {
  detach_all(scene);

  objs_.clear();
  transform.reset_local(eray::math::Vec3f::filled(0.F));
}

void SceneObjectsSelection::set_custom_origin(Scene& scene, eray::math::Vec3f vec) {
  if (use_custom_origin_) {
    detach_all(scene);
    transform.reset_local(vec);
  }

  custom_origin_ = vec;
}

void SceneObjectsSelection::use_custom_origin(Scene& scene, bool use_custom_origin) {
  if (use_custom_origin == use_custom_origin_) {
    return;
  }

  detach_all(scene);

  use_custom_origin_ = use_custom_origin;
  if (!use_custom_origin_) {
    transform.reset_local(centroid_);
  }
}

void SceneObjectsSelection::update_transforms(Scene& scene, Cursor& cursor) {
  if (use_custom_origin_) {
    update_centroid(scene);
    cursor.transform.set_local_pos(transform.pos());
    cursor.mark_dirty();
  } else {
    centroid_ = transform.pos();
  }

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

void SceneObjectsSelection::detach_all(Scene& scene) {
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

void SceneObjectsSelection::update_centroid(Scene& scene) {
  centroid_ = eray::math::Vec3f::filled(0.F);
  if (is_empty()) {
    if (!use_custom_origin_) {
      transform.reset_local(centroid_);
    }
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
  if (!use_custom_origin_) {
    transform.reset_local(centroid_);
  }
}

}  // namespace mini
