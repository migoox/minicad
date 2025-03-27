#include <expected>
#include <liberay/util/panic.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <memory>
#include <optional>
#include <variant>

namespace mini {

std::uint32_t Scene::next_signature_ = 0;

Scene::Scene() : signature_(next_signature_++), curr_timestamp_(0) { scene_objects_.resize(kMaxSceneObjects); }

bool Scene::is_handle_valid(const SceneObjectHandle& handle) {
  if (handle.owner_signature != signature_) {
    return false;
  }

  if (handle.obj_id > kMaxSceneObjects) {
    return false;
  }

  if (!scene_objects_[handle.obj_id]) {
    return false;
  }

  if (scene_objects_[handle.obj_id]->second != handle.timestamp) {
    return false;
  }

  return true;
}

bool Scene::is_handle_valid(const PointHandle& handle) {
  if (!is_handle_valid(SceneObjectHandle(handle.owner_signature, handle.timestamp, handle.obj_id))) {
    return false;
  }

  return std::holds_alternative<Point>(scene_objects_[handle.obj_id]->first->object);
}

bool Scene::is_handle_valid(const PointListObjectHandle& handle) {
  if (handle.owner_signature != signature_) {
    return false;
  }

  if (handle.obj_id > kMaxSceneObjects) {
    return false;
  }

  if (!point_list_objects_[handle.obj_id]) {
    return false;
  }

  if (point_list_objects_[handle.obj_id]->second != handle.timestamp) {
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

SceneObjectHandle Scene::create_scene_obj_handle(std::uint32_t obj_id) {
  if (!scene_objects_[obj_id]) {
    eray::util::panic("Scene Object with id={} does not exist", obj_id);
  }
  return SceneObjectHandle(signature_, scene_objects_[obj_id]->second, obj_id);
}

std::expected<SceneObjectHandle, Scene::ObjectCreationError> Scene::create_scene_obj() {
  if (scene_objects_freed_.empty()) {
    return std::unexpected(ObjectCreationError::ReachedMaxObjects);
  }

  auto obj_id            = scene_objects_freed_.top();
  scene_objects_[obj_id] = std::pair(std::make_unique<SceneObject>(obj_id), timestamp());
  return create_scene_obj_handle(obj_id);
}

bool Scene::delete_obj(const SceneObjectHandle& handle) {
  if (!is_handle_valid(handle)) {
    return false;
  }

  if (auto* p = std::get_if<Point>(&scene_objects_[handle.obj_id]->first->object)) {
    for (const auto& pl : p->point_lists_) {
      remove_point_from_list(PointHandle(pl.owner_signature, pl.timestamp, pl.obj_id), pl);
    }
  }

  scene_objects_[handle.obj_id] = std::nullopt;
  scene_objects_freed_.pop();
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

  point_list_objects_[handle.obj_id] = std::nullopt;
  point_list_objects_freed_.push(handle.obj_id);
  return true;
}

std::expected<PointListObjectHandle, Scene::ObjectCreationError> Scene::create_list_obj() {
  if (point_list_objects_.empty()) {
    return std::unexpected(ObjectCreationError::ReachedMaxObjects);
  }

  auto obj_id = point_list_objects_freed_.top();
  point_list_objects_freed_.pop();
  point_list_objects_[obj_id] = std::pair(std::make_unique<PointListObject>(obj_id), timestamp());
  return create_list_obj_handle(obj_id);
}

std::optional<PointHandle> Scene::get_point_handle(const SceneObjectHandle& handle) {
  auto p = PointHandle(handle.owner_signature, handle.timestamp, handle.obj_id);
  if (!is_handle_valid(p)) {
    return std::nullopt;
  }

  return p;
}

bool Scene::add_point_to_list(const PointHandle& p_handle, const PointListObjectHandle& pl_handle) {
  if (!is_handle_valid(p_handle) || !is_handle_valid(pl_handle)) {
    return false;
  }

  if (auto* p = std::get_if<Point>(&scene_objects_[p_handle.obj_id]->first->object)) {
    if (p->point_lists_.contains(pl_handle)) {
      return false;
    }

    p->point_lists_.insert(pl_handle);
    auto& pl = *point_list_objects_[pl_handle.obj_id]->first;
    pl.points_.push_back(p_handle);
    pl.points_map_.insert({p_handle, std::prev(pl.points_.end())});

    return true;
  }

  eray::util::panic("Provided point handle does not refers to a point");
  return false;
}

bool Scene::remove_point_from_list(const PointHandle& p_handle, const PointListObjectHandle& pl_handle) {
  if (!is_handle_valid(p_handle) || !is_handle_valid(pl_handle)) {
    return false;
  }

  if (auto* p = std::get_if<Point>(&scene_objects_[p_handle.obj_id]->first->object)) {
    if (p->point_lists_.contains(pl_handle)) {
      return false;
    }

    p->point_lists_.erase(pl_handle);
    auto& pl  = *point_list_objects_[pl_handle.obj_id]->first;
    auto p_it = pl.points_map_.find(p_handle);
    pl.points_.erase(p_it->second);
    pl.points_map_.erase(p_it);

    return true;
  }

  eray::util::panic("Provided point handle does not refers to a point");
  return false;
}

}  // namespace mini
