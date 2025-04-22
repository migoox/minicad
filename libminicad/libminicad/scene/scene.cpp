#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <memory>
#include <optional>
#include <variant>

#include "libminicad/renderer/rendering_command.hpp"

namespace mini {

std::uint32_t Scene::next_signature_ = 0;

Scene::Scene(std::unique_ptr<ISceneRenderer>&& renderer)
    : renderer_(std::move(renderer)),
      signature_(next_signature_++),
      curr_timestamp_(0),
      curr_scene_obj_ind_(1),
      curr_list_obj_ind_(1) {
  scene_objects_.resize(kMaxObjects);
  point_list_objects_.resize(kMaxObjects);
  for (size_t i = kMaxObjects; i > 0; --i) {
    scene_objects_freed_.push(static_cast<uint32_t>(i - 1));
    point_list_objects_freed_.push(static_cast<uint32_t>(i - 1));
  }
}

std::optional<SceneObjectHandle> Scene::handle_by_obj_id(SceneObjectId id) {
  if (id >= kMaxObjects) {
    return std::nullopt;
  }

  if (!scene_objects_[id]) {
    return std::nullopt;
  }

  return scene_objects_[id]->first->handle();
}

bool Scene::is_handle_valid(const SceneObjectHandle& handle) const {
  if (handle.owner_signature != signature_) {
    return false;
  }

  if (handle.obj_id > kMaxObjects) {
    return false;
  }

  if (!scene_objects_[handle.obj_id]) {
    return false;
  }

  if (scene_objects_[handle.obj_id]->second.first != handle.timestamp) {
    return false;
  }

  return true;
}

bool Scene::is_handle_valid(const PointListObjectHandle& handle) const {
  if (handle.owner_signature != signature_) {
    return false;
  }

  if (handle.obj_id > kMaxObjects) {
    return false;
  }

  if (!point_list_objects_[handle.obj_id]) {
    return false;
  }

  if (point_list_objects_[handle.obj_id]->second.first != handle.timestamp) {
    return false;
  }

  return true;
}

std::uint32_t Scene::timestamp() { return curr_timestamp_++; }

OptionalObserverPtr<SceneObject> Scene::get_obj(const SceneObjectHandle& handle) {
  if (!is_handle_valid(handle)) {
    return std::nullopt;
  }

  return OptionalObserverPtr<SceneObject>(*scene_objects_[handle.obj_id]->first);
}

bool Scene::add_to_list(const SceneObjectHandle& p_handle, const PointListObjectHandle& pl_handle) {
  if (!is_handle_valid(pl_handle)) {
    return false;
  }

  if (std::holds_alternative<Point>(scene_objects_[p_handle.obj_id]->first->object)) {
    return point_list_objects_[pl_handle.obj_id]->first->add(p_handle).has_value();
  }

  return false;
}

bool Scene::remove_from_list(const SceneObjectHandle& p_handle, const PointListObjectHandle& pl_handle) {
  if (!is_handle_valid(pl_handle)) {
    return false;
  }

  if (std::holds_alternative<Point>(scene_objects_[p_handle.obj_id]->first->object)) {
    return point_list_objects_[pl_handle.obj_id]->first->remove(p_handle).has_value();
  }

  return false;
}

std::expected<SceneObjectHandle, Scene::ObjectCreationError> Scene::create_scene_obj(SceneObjectVariant variant) {
  if (scene_objects_freed_.empty()) {
    eray::util::Logger::warn("Reached limit of scene objects");
    return std::unexpected(ObjectCreationError::ReachedMaxObjects);
  }

  auto obj_id = scene_objects_freed_.top();
  scene_objects_freed_.pop();
  auto h = SceneObjectHandle(signature_, timestamp(), obj_id);
  scene_objects_list_.push_back(h);
  scene_objects_[obj_id] =
      std::pair(std::make_unique<SceneObject>(h, *this), std::pair(h.timestamp, std::prev(scene_objects_list_.end())));
  dirty_scene_objects_.insert(obj_id);

  auto& obj    = *scene_objects_[obj_id]->first;
  auto obj_ind = curr_scene_obj_ind_++;
  std::visit(eray::util::match{
                 [&obj, obj_ind](Point&) { obj.name = std::format("Point {}", obj_ind); },
                 [&obj, obj_ind](Torus&) { obj.name = std::format("Torus {}", obj_ind); },
             },
             variant);
  obj.object = std::move(variant);

  add_to_order(obj);

  renderer_->push_object_rs_cmd(SceneObjectRSCommand(h, SceneObjectRSCommand::Internal::AddObject{}));
  return h;
}

void Scene::add_to_order(SceneObject& obj) {
  obj.order_ind_ = objects_order_.size();
  objects_order_.emplace_back(obj.handle());
}

void Scene::add_to_order(PointListObject& obj) {
  obj.order_ind_ = objects_order_.size();
  objects_order_.emplace_back(obj.handle());
}

void Scene::remove_from_order(size_t ind) {
  for (size_t i = ind + 1; i < objects_order_.size(); ++i) {
    std::visit(
        eray::util::match{
            [&](const SceneObjectHandle& handle) { scene_objects_.at(handle.obj_id)->first->order_ind_--; },
            [&](const PointListObjectHandle& handle) { point_list_objects_.at(handle.obj_id)->first->order_ind_--; }},
        objects_order_[i]);
  }
  objects_order_.erase(objects_order_.begin() + ind);
}

bool Scene::delete_obj(const SceneObjectHandle& handle) {
  if (!is_handle_valid(handle)) {
    return false;
  }

  auto& obj = *scene_objects_[handle.obj_id]->first;
  remove_from_order(obj.order_ind_);
  scene_objects_list_.erase(scene_objects_[handle.obj_id]->second.second);
  scene_objects_[handle.obj_id] = std::nullopt;
  scene_objects_freed_.push(handle.obj_id);
  return true;
}

OptionalObserverPtr<PointListObject> Scene::get_obj(const PointListObjectHandle& handle) {
  if (!is_handle_valid(handle)) {
    return std::nullopt;
  }

  return OptionalObserverPtr<PointListObject>(*point_list_objects_[handle.obj_id]->first);
}

bool Scene::delete_obj(const PointListObjectHandle& handle) {
  if (!is_handle_valid(handle)) {
    return false;
  }

  auto& obj = *point_list_objects_[handle.obj_id]->first;
  remove_from_order(obj.order_ind_);
  point_list_objects_list_.erase(point_list_objects_[handle.obj_id]->second.second);
  point_list_objects_[handle.obj_id] = std::nullopt;
  point_list_objects_freed_.push(handle.obj_id);
  return true;
}

std::expected<PointListObjectHandle, Scene::ObjectCreationError> Scene::create_list_obj(
    PointListObjectVariant variant) {
  if (point_list_objects_.empty()) {
    eray::util::Logger::warn("Reached limit of scene objects");
    return std::unexpected(ObjectCreationError::ReachedMaxObjects);
  }

  auto obj_id = point_list_objects_freed_.top();
  point_list_objects_freed_.pop();
  auto h = PointListObjectHandle(signature_, timestamp(), obj_id);
  point_list_objects_list_.push_back(h);
  point_list_objects_[obj_id] = std::pair(std::make_unique<PointListObject>(h, *this),
                                          std::pair(h.timestamp, std::prev(point_list_objects_list_.end())));
  dirty_point_list_objects_.insert(obj_id);

  auto& obj    = *point_list_objects_[obj_id]->first;
  auto obj_ind = curr_list_obj_ind_++;
  std::visit(eray::util::match{
                 [&obj, obj_ind](Polyline&) { obj.name = std::format("Polyline {}", obj_ind); },
                 [&obj, obj_ind](MultisegmentBezierCurve&) { obj.name = std::format("Bezier {}", obj_ind); },
             },
             variant);
  obj.object = std::move(variant);

  add_to_order(obj);

  renderer_->push_object_rs_cmd(PointListObjectRSCommand(h, PointListObjectRSCommand::Internal::AddObject{}));
  return h;
}

}  // namespace mini
