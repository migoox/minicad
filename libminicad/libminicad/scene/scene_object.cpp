#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <variant>

namespace mini {

namespace util = eray::util;

SceneObject::~SceneObject() {
  for (const auto& pl_h : point_lists_) {
    if (auto pl = scene_.get_obj(pl_h)) {
      if (!pl.value()->remove(handle_)) {
        util::Logger::warn("Could not remove {} from list object.", pl.value()->name);
      }
    }
  }
}

void SceneObject::mark_dirty() { scene_.mark_dirty(handle_); }

PointListObject::~PointListObject() {
  for (auto p : points_) {
    p.get().point_lists_.erase(handle_);
  }
}

void PointListObject::mark_dirty() { scene_.mark_dirty(handle_); }

std::expected<void, PointListObject::SceneObjectError> PointListObject::add(const SceneObjectHandle& handle) {
  if (points_map_.contains(handle)) {
    return {};
  }

  if (!scene_.is_handle_valid(handle)) {
    return std::unexpected(SceneObjectError::HandleIsNotValid);
  }

  auto& obj = *scene_.scene_objects_.at(handle.obj_id)->first;
  if (std::holds_alternative<Point>(obj.object)) {
    auto ind = points_.size();
    points_map_.insert({handle, ind});
    points_.emplace_back(obj);

    obj.point_lists_.insert(handle_);

    mark_dirty();

    return {};
  }

  auto obj_type_name = std::visit(util::match{[&](auto& p) { return p.type_name(); }}, obj.object);
  util::Logger::warn(
      "Detected an attempt to add a scene object that holds type \"{}\" to point list. Only objects of type \"{}\" "
      "are accepted.",
      obj_type_name, Point::type_name());

  return std::unexpected(SceneObjectError::NotAPoint);
}

std::expected<void, PointListObject::SceneObjectError> PointListObject::remove(const SceneObjectHandle& handle) {
  if (!scene_.is_handle_valid(handle)) {
    return std::unexpected(SceneObjectError::HandleIsNotValid);
  }

  if (!points_map_.contains(handle)) {
    return std::unexpected(SceneObjectError::NotFound);
  }

  auto& obj = *scene_.scene_objects_.at(handle.obj_id)->first;
  if (std::holds_alternative<Point>(obj.object)) {
    auto it = points_map_.find(handle);
    if (it != points_map_.end()) {
      auto ind = it->second;

      points_map_.erase(it);
      points_.erase(points_.begin() + ind);
    }

    auto obj_it = obj.point_lists_.find(handle_);
    if (obj_it != obj.point_lists_.end()) {
      obj.point_lists_.erase(obj_it);
    }
    mark_dirty();

    return {};
  }

  return std::unexpected(SceneObjectError::NotAPoint);
}

}  // namespace mini
