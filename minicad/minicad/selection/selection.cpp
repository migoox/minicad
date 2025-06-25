#include <liberay/math/vec_fwd.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/handles.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/types.hpp>
#include <minicad/selection/selection.hpp>
#include <optional>
#include <variant>

namespace mini {

using Vec3f = eray::math::Vec3f;

void TransformableSelection::remove(Scene& scene, const TransformableObjectHandle& handle) {
  auto f = objs_.find(handle);
  if (f == objs_.end()) {
    return;
  }
  detach_all(scene);

  objs_.erase(objs_.find(handle));
  update_centroid(scene);
  update_is_points_only_selection();
}

void TransformableSelection::add(Scene& scene, const TransformableObjectHandle& handle) {
  if (points_only_ || objs_.empty()) {
    points_only_ = std::holds_alternative<PointObjectHandle>(handle);
  }

  detach_all(scene);

  objs_.insert(handle);
  update_centroid(scene);
}

void TransformableSelection::clear(Scene& scene) {
  points_only_ = false;
  detach_all(scene);

  objs_.clear();
  transform.reset_local(eray::math::Vec3f::filled(0.F));
}

void TransformableSelection::set_custom_origin(Scene& scene, eray::math::Vec3f vec) {
  if (use_custom_origin_) {
    detach_all(scene);
    transform.reset_local(vec);
  }

  custom_origin_ = vec;
}

void TransformableSelection::use_custom_origin(Scene& scene, bool use_custom_origin) {
  if (use_custom_origin == use_custom_origin_) {
    return;
  }

  detach_all(scene);

  use_custom_origin_ = use_custom_origin;
  if (!use_custom_origin_) {
    transform.reset_local(centroid_);
  }
}

void TransformableSelection::update_transforms(Scene& scene, Cursor& cursor) {
  if (use_custom_origin_) {
    update_centroid(scene);
    cursor.transform.set_local_pos(transform.pos());
    cursor.mark_dirty();
  } else {
    centroid_ = transform.pos();
  }

  auto obj_update = [&](const auto& handle) {
    using T = ERAY_HANDLE_OBJ(handle);
    if (auto opt = scene.arena<T>().get_obj(handle)) {
      CObject auto& obj = **opt;
      obj.update();
    }
  };

  for (const auto& handle : objs_) {
    std::visit(eray::util::match{obj_update}, handle);
  }

  if (transform_dirty_) {
    return;
  }

  auto obj_set_parent = [&](const auto& handle) {
    using T = ERAY_HANDLE_OBJ(handle);
    if (auto opt = scene.arena<T>().get_obj(handle)) {
      CTransformableObject auto& obj = **opt;
      obj.transform().set_parent(transform);
    }
  };

  transform_dirty_ = true;
  for (const auto& handle : objs_) {
    std::visit(eray::util::match{obj_set_parent}, handle);
  }
}

void TransformableSelection::detach_all(Scene& scene) {
  auto detach_from_parent = [&](const auto& handle) {
    using T = ERAY_HANDLE_OBJ(handle);
    if (auto opt = scene.arena<T>().get_obj(handle)) {
      CTransformableObject auto& obj = **opt;
      obj.transform().detach_from_parent();
      obj.update();
    }
  };

  if (!transform_dirty_) {
    return;
  }
  transform_dirty_ = false;

  for (const auto& handle : objs_) {
    std::visit(eray::util::match{detach_from_parent}, handle);
  }
}

void TransformableSelection::update_centroid(Scene& scene) {
  centroid_ = eray::math::Vec3f::filled(0.F);
  if (is_empty()) {
    if (!use_custom_origin_) {
      transform.reset_local(centroid_);
    }
    return;
  }

  int count            = 0;
  auto update_centroid = [&](const auto& handle) {
    using T = ERAY_HANDLE_OBJ(handle);
    if (auto opt = scene.arena<T>().get_obj(handle)) {
      CTransformableObject auto& obj = **opt;
      centroid_ += obj.transform().pos();
      count++;
    }
  };
  for (const auto& obj : objs_) {
    std::visit(eray::util::match{update_centroid}, obj);
  }

  centroid_ /= static_cast<float>(count);
  if (!use_custom_origin_) {
    transform.reset_local(centroid_);
  }
}

void TransformableSelection::update_is_points_only_selection() {
  points_only_ = true;

  for (const auto& h : objs_) {
    if (!std::holds_alternative<PointObjectHandle>(h)) {
      points_only_ = false;
      return;
    }
  }
}

std::optional<eray::math::Vec3f> HelperPointSelection::pos(Scene& scene) {
  if (!selection_) {
    return std::nullopt;
  }

  if (auto o = scene.arena<Curve>().get_obj(selection_->parent)) {
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
  if (auto o = scene.arena<Curve>().get_obj(selection_->parent)) {
    if (auto* bspline = std::get_if<BSplineCurve>(&o.value()->object)) {
      bspline->set_bernstein_point(*o.value(), selection_->helper_point, new_pos);
      scene.renderer().push_object_rs_cmd(CurveRSCommand(selection_->parent, CurveRSCommand::UpdateHelperPoints{}));
    }
  }
}

}  // namespace mini
