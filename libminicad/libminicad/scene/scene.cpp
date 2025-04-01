#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <memory>
#include <optional>
#include <variant>

namespace mini {

std::uint32_t Scene::next_signature_ = 0;

Scene::Scene() : signature_(next_signature_++), curr_timestamp_(0), curr_scene_obj_ind_(1), curr_list_obj_ind_(1) {
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

bool Scene::is_handle_valid(const PointHandle& handle) const {
  if (!is_handle_valid(SceneObjectHandle(handle.owner_signature, handle.timestamp, handle.obj_id))) {
    return false;
  }

  return std::holds_alternative<Point>(scene_objects_[handle.obj_id]->first->object);
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

OptionalObserverPtr<SceneObject> Scene::get_obj(const PointHandle& handle) {
  if (!is_handle_valid(handle)) {
    return std::nullopt;
  }

  return OptionalObserverPtr<SceneObject>(*scene_objects_[handle.obj_id]->first);
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

  return h;
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
  point_list_objects_list_.erase(point_list_objects_[handle.obj_id]->second.second);
  point_list_objects_[handle.obj_id] = std::nullopt;
  point_list_objects_freed_.push(handle.obj_id);
  return true;
}

std::expected<PointListObjectHandle, Scene::ObjectCreationError> Scene::create_list_obj() {
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
  obj.name     = std::format("Point List {}", obj_ind);

  return h;
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
    mark_dirty(pl_handle);

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
    if (!p->point_lists_.contains(pl_handle)) {
      return false;
    }

    p->point_lists_.erase(pl_handle);
    auto& pl  = *point_list_objects_[pl_handle.obj_id]->first;
    auto p_it = pl.points_map_.find(p_handle);
    pl.points_.erase(p_it->second);
    pl.points_map_.erase(p_it);
    mark_dirty(pl_handle);

    return true;
  }

  eray::util::panic("Provided point handle does not refers to a point");
  return false;
}
void Scene::visit_dirty_scene_objects(const std::function<void(SceneObject&)>& visitor) {
  for (auto id : dirty_scene_objects_) {
    if (scene_objects_[id]) {
      visitor(*scene_objects_[id]->first);
    }
  }
  dirty_scene_objects_.clear();
}

void Scene::visit_dirty_point_objects(const std::function<void(PointListObject&)>& visitor) {
  for (auto id : dirty_point_list_objects_) {
    if (point_list_objects_[id]) {
      visitor(*point_list_objects_[id]->first);
    }
  }
  dirty_point_list_objects_.clear();
}

}  // namespace mini
