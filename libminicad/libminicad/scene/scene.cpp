#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

#include "libminicad/scene/scene_object_handle.hpp"

namespace mini {

std::uint32_t Scene::next_signature_ = 0;

Scene::Scene(std::unique_ptr<ISceneRenderer>&& renderer)
    : renderer_(std::move(renderer)), signature_(next_signature_++) {
  arena<SceneObject>().init(kMaxObjects, signature_);
  arena<Curve>().init(kMaxObjects, signature_);
  arena<PatchSurface>().init(kMaxObjects, signature_);
}

bool Scene::add_point_to_curve(const SceneObjectHandle& p_handle, const CurveHandle& c_handle) {
  if (auto c = arena<Curve>().get_obj(c_handle)) {
    if (auto p = arena<SceneObject>().get_obj(p_handle)) {
      if (p.value()->has_type<Point>()) {
        return c.value()->add(p_handle).has_value();
      }
    }
  }
  return false;
}

bool Scene::remove_point_from_curve(const SceneObjectHandle& p_handle, const CurveHandle& c_handle) {
  if (auto c = arena<Curve>().get_obj(c_handle)) {
    if (auto p = arena<SceneObject>().get_obj(p_handle)) {
      if (p.value()->has_type<Point>()) {
        return c.value()->remove(p_handle).has_value();
      }
    }
  }

  return false;
}

std::expected<SceneObjectHandle, Scene::ObjectCreationError> Scene::create_scene_obj(SceneObjectVariant variant) {
  auto h = arena<SceneObject>().create(*this);
  if (!h) {
    return std::unexpected(ObjectCreationError::ReachedMaxObjects);
  }
  const auto& handle = *h;
  auto& obj          = arena<SceneObject>().at_unsafe(handle);
  auto obj_ind       = arena<SceneObject>().curr_obj_idx();
  std::visit(eray::util::match{
                 [&](Point&) { obj.name = std::format("Point {}", obj_ind); },
                 [&](Torus&) { obj.name = std::format("Torus {}", obj_ind); },
             },
             variant);
  obj.object = std::move(variant);

  obj.order_idx_ = objects_order_.size();
  objects_order_.emplace_back(handle);

  renderer_->push_object_rs_cmd(SceneObjectRSCommand(handle, SceneObjectRSCommand::Internal::AddObject{}));
  return handle;
}

std::expected<CurveHandle, Scene::ObjectCreationError> Scene::create_curve(CurveVariant variant) {
  auto h = arena<Curve>().create(*this);
  if (!h) {
    return std::unexpected(ObjectCreationError::ReachedMaxObjects);
  }
  const auto& handle = *h;
  auto& obj          = arena<Curve>().at_unsafe(handle);
  auto obj_ind       = arena<Curve>().curr_obj_idx();
  std::visit(eray::util::match{
                 [&obj, obj_ind](Polyline&) { obj.name = std::format("Polyline {}", obj_ind); },
                 [&obj, obj_ind](MultisegmentBezierCurve&) { obj.name = std::format("Bezier {}", obj_ind); },
                 [&obj, obj_ind](BSplineCurve&) { obj.name = std::format("B-Spline {}", obj_ind); },
                 [&obj, obj_ind](NaturalSplineCurve&) { obj.name = std::format("Natural Spline {}", obj_ind); },
             },
             variant);
  obj.object = std::move(variant);

  obj.order_idx_ = objects_order_.size();
  objects_order_.emplace_back(handle);

  renderer_->push_object_rs_cmd(CurveRSCommand(handle, CurveRSCommand::Internal::AddObject{}));
  return handle;
}

std::expected<PatchSurfaceHandle, Scene::ObjectCreationError> Scene::create_patch_surface(PatchSurfaceVariant variant) {
  auto h = arena<PatchSurface>().create(*this);
  if (!h) {
    return std::unexpected(ObjectCreationError::ReachedMaxObjects);
  }
  const auto& handle = *h;
  auto& obj          = arena<PatchSurface>().at_unsafe(handle);
  auto obj_ind       = arena<PatchSurface>().curr_obj_idx();
  std::visit(eray::util::match{
                 [&obj, obj_ind](BezierPatches&) { obj.name = std::format("Bezier Patches {}", obj_ind); },
                 [&obj, obj_ind](BPatches&) { obj.name = std::format("B-Patches {}", obj_ind); },
             },
             variant);
  obj.object = std::move(variant);

  obj.order_idx_ = objects_order_.size();
  objects_order_.emplace_back(handle);

  renderer_->push_object_rs_cmd(PatchSurfaceRSCommand(handle, PatchSurfaceRSCommand::Internal::AddObject{}));
  return handle;
}

void Scene::remove_from_order(size_t ind) {
  for (size_t i = ind + 1; i < objects_order_.size(); ++i) {
    std::visit(eray::util::match{[&](const auto& handle) {
                 using T = std::decay_t<decltype(handle)>::Object;
                 arena<T>().at_unsafe(handle).order_idx_--;
               }},
               objects_order_[i]);
  }
  objects_order_.erase(objects_order_.begin() + static_cast<int>(ind));
}

}  // namespace mini
