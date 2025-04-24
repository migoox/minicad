#include <cstddef>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/scene/scene.hpp>
#include <minicad/selection/selection.hpp>
#include <optional>
#include <variant>

#include "libminicad/renderer/rendering_command.hpp"
#include "libminicad/scene/scene_object.hpp"

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
  update_is_points_only_selection(scene);
}

void SceneObjectsSelection::add(Scene& scene, const SceneObjectHandle& handle) {
  if (points_only_ || objs_.empty()) {
    bool is_point = false;
    if (auto o = scene.get_obj(handle)) {
      is_point = std::holds_alternative<Point>(o.value()->object);
    } else {
      return;
    }

    points_only_ = is_point;
  }

  detach_all(scene);

  objs_.insert(handle);
  update_centroid(scene);
}

void SceneObjectsSelection::clear(Scene& scene) {
  points_only_ = false;
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

void SceneObjectsSelection::update_is_points_only_selection(Scene& scene) {
  points_only_ = true;
  for (const auto& h : objs_) {
    if (auto o = scene.get_obj(h)) {
      if (!std::holds_alternative<Point>(o.value()->object)) {
        points_only_ = false;
        return;
      }
    }
  }
}

std::optional<eray::math::Vec3f> HelperPointSelection::pos(Scene& scene) {
  if (!selection_) {
    return std::nullopt;
  }

  if (auto o = scene.get_obj(selection_->parent)) {
    if (auto* bspline = std::get_if<BSplineCurve>(&o.value()->object)) {
      return bspline->bernstein_point(selection_->helper_point);
    }
  }
  selection_ = std::nullopt;
  return std::nullopt;
}

void HelperPointSelection::set_point(Scene& scene, eray::math::Vec3f new_pos) {
  if (!selection_) {
    return;
  }
  if (auto o = scene.get_obj(selection_->parent)) {
    if (auto* bspline = std::get_if<BSplineCurve>(&o.value()->object)) {
      bspline->set_bernstein_point(*o.value(), selection_->helper_point, new_pos);
      scene.renderer().push_object_rs_cmd(
          PointListObjectRSCommand(selection_->parent, PointListObjectRSCommand::UpdateBernsteinControlPoints{}));
    }
  }
}

}  // namespace mini
