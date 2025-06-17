#include <expected>
#include <generator>
#include <liberay/math/vec.hpp>
#include <liberay/util/container_extensions.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/curve.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <optional>
#include <unordered_set>
#include <variant>
#include <vector>

namespace mini {

namespace util = eray::util;

SceneObject::SceneObject(SceneObjectHandle handle, Scene& scene)
    : ObjectBase<SceneObject, SceneObjectVariant>(handle, scene) {
  scene.renderer().push_object_rs_cmd(SceneObjectRSCommand(handle, SceneObjectRSCommand::Internal::AddObject{}));
}

bool SceneObject::can_be_deleted() const { return patch_surfaces_.empty(); }

void SceneObject::on_delete() {
  if (!can_be_deleted()) {
    eray::util::Logger::err("Requested deletion of a scene object, however it cannot be deleted.");
    return;
  }

  scene().renderer().push_object_rs_cmd(SceneObjectRSCommand(handle_, SceneObjectRSCommand::Internal::DeleteObject{}));

  if (has_type<Point>()) {
    // Only points might be a part of the curve

    for (const auto& c_h : curves_) {
      if (auto pl = scene().arena<Curve>().get_obj(c_h)) {
        if (pl.value()->points_.remove(*this)) {
          pl.value()->update();
          std::visit(eray::util::match{[&](auto& obj) {
                       obj.on_point_remove(*pl.value(), *this, unsafe_get_variant<Point>());
                     }},
                     pl.value()->object);
        }
      }
    }
  }
}

void SceneObject::update() {
  scene().renderer().push_object_rs_cmd(SceneObjectRSCommand(handle_, SceneObjectRSCommand::UpdateObjectMembers{}));

  // Only points might be a part of the point lists
  if (has_type<Point>()) {
    for (const auto& c_h : this->curves_) {
      if (auto pl = scene().arena<Curve>().get_obj(c_h)) {
        scene().renderer().push_object_rs_cmd(CurveRSCommand(c_h, CurveRSCommand::Internal::UpdateControlPoints{}));
        std::visit(
            eray::util::match{[&](auto& obj) { obj.on_point_update(*pl.value(), *this, unsafe_get_variant<Point>()); }},
            pl.value()->object);
      }
    }
    for (const auto& ps_h : this->patch_surfaces_) {
      if (auto ps = scene().arena<PatchSurface>().get_obj(ps_h)) {
        auto& obj = **ps;
        obj.mark_bezier3_dirty();
        scene().renderer().push_object_rs_cmd(
            PatchSurfaceRSCommand(ps_h, PatchSurfaceRSCommand::Internal::UpdateControlPoints{}));
      }
    }

    for (const auto& fs_h : this->fill_in_surfaces_) {
      if (auto opt = scene().arena<FillInSurface>().get_obj(fs_h)) {
        auto& obj = **opt;
        obj.mark_points_dirty();
        scene().renderer().push_object_rs_cmd(
            FillInSurfaceRSCommand(fs_h, FillInSurfaceRSCommand::Internal::UpdateControlPoints{}));
      }
    }
  }
}

void SceneObject::move_refs_to(SceneObject& obj) {
  for (const auto& c_h : curves_) {
    if (auto pl = scene().arena<Curve>().get_obj(c_h)) {
      if (auto result = pl.value()->points_.replace(handle(), obj); !result) {
        util::Logger::warn("Could not replace the scene object");
      }

      obj.curves_.insert(c_h);

      scene().renderer().push_object_rs_cmd(CurveRSCommand(c_h, CurveRSCommand::Internal::UpdateControlPoints{}));
      std::visit(
          eray::util::match{[&](auto& obj) { obj.on_point_update(*pl.value(), *this, unsafe_get_variant<Point>()); }},
          pl.value()->object);
    }
  }

  for (const auto& ps_h : patch_surfaces_) {
    if (auto ps = scene().arena<PatchSurface>().get_obj(ps_h)) {
      auto& ps_obj = **ps;

      if (auto result = ps_obj.points_.replace(handle(), obj); !result) {
        util::Logger::warn("Could not replace the scene object");
      }
      obj.patch_surfaces_.insert(ps_h);

      scene().renderer().push_object_rs_cmd(
          PatchSurfaceRSCommand(ps_h, PatchSurfaceRSCommand::Internal::UpdateControlPoints{}));
      ps_obj.mark_bezier3_dirty();
    }
  }

  for (const auto& fs_h : fill_in_surfaces_) {
    if (auto fs = scene().arena<FillInSurface>().get_obj(fs_h)) {
      auto& fs_obj = **fs;

      if (auto result = fs_obj.replace(handle(), obj); !result) {
        util::Logger::warn("Could not replace the scene object");
      }
      obj.fill_in_surfaces_.insert(fs_h);

      scene().renderer().push_object_rs_cmd(
          FillInSurfaceRSCommand(fs_h, FillInSurfaceRSCommand::Internal::UpdateControlPoints{}));
      fs_obj.mark_points_dirty();
    }
  }

  curves_.clear();
  patch_surfaces_.clear();
  fill_in_surfaces_.clear();
}

void SceneObject::clone_to(SceneObject& obj) const {
  obj.transform = this->transform.clone_detached();
  obj.name      = this->name + " Copy";
  obj.object    = this->object;

  scene_.get().renderer().push_object_rs_cmd(
      SceneObjectRSCommand(obj.handle(), SceneObjectRSCommand::UpdateObjectMembers{}));
}

}  // namespace mini
