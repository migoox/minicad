#include <expected>
#include <liberay/util/logger.hpp>
#include <libminicad/scene/point_list.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <unordered_map>
#include <unordered_set>

namespace mini {

std::expected<void, PointList::OperationError> PointList::push_back(SceneObject& obj) {
  if (!obj.has_type<Point>()) {
    eray::util::Logger::err("Could not push back a scene object as it's not a point");
    return std::unexpected(OperationError::NotAPoint);
  }

  auto idx    = points_.size();
  auto obj_it = points_map_.find(obj.handle());
  if (obj_it != points_map_.end()) {
    points_.emplace_back(obj);
    obj_it->second.emplace(idx);
  } else {
    auto [it, success] = points_map_.emplace(obj.handle(), std::unordered_set<size_t>());
    if (!success) {
      eray::util::Logger::err("Could not replace a point. Map insertion failed.");
      return std::unexpected(OperationError::MapInsertionFailure);
    }
    it->second.emplace(idx);
    points_.emplace_back(obj);
  }

  return {};
}

void PointList::unsafe_set(Scene& scene, const std::vector<SceneObjectHandle>& handles) {
  clear();
  for (const auto& handle : handles) {
    auto& obj = *scene.arena<SceneObject>().unsafe_get_obj(handle);
    if (!push_back(obj)) {
      eray::util::Logger::err("Could not push back");
    }
  }
}

void PointList::clear() {
  points_.clear();
  points_map_.clear();
}

std::expected<void, PointList::OperationError> PointList::remove(SceneObject& obj) {
  auto it = points_map_.find(obj.handle());
  if (it == points_map_.end()) {
    return std::unexpected(OperationError::NotFound);
  }
  points_map_.erase(it);

  int count = 0;
  for (auto i = 0U, j = 0U; i < points_.size(); ++i) {
    if (&points_[i].get() != &obj) {  // if valid
      points_[j++] = points_[i];
    } else {
      ++count;
    }
  }
  points_.erase(points_.end() - count, points_.end());

  update_indices();

  return {};
}

std::expected<bool, PointList::OperationError> PointList::move_before(size_t dest_idx, size_t source_idx) {
  if (dest_idx >= points_.size() || source_idx >= points_.size()) {
    return std::unexpected(OperationError::NotFound);
  }

  if (source_idx == dest_idx) {
    return false;
  }

  SceneObject& obj_ref = points_[source_idx].get();
  points_.erase(points_.begin() + static_cast<int>(source_idx));
  if (source_idx < dest_idx && dest_idx > 0) {
    dest_idx--;
  }
  points_.insert(points_.begin() + static_cast<int>(dest_idx), std::ref(obj_ref));
  update_indices();

  return true;
}

std::expected<bool, PointList::OperationError> PointList::move_after(size_t dest_idx, size_t source_idx) {
  if (dest_idx >= points_.size() || source_idx >= points_.size()) {
    return std::unexpected(OperationError::NotFound);
  }

  if (source_idx == dest_idx + 1) {
    return false;
  }

  SceneObject& obj_ref = points_[source_idx].get();
  points_.erase(points_.begin() + static_cast<int>(source_idx));
  size_t insert_pos = dest_idx + 1;
  if (source_idx > dest_idx && insert_pos > 0) {
    insert_pos--;
  }
  insert_pos = std::min(insert_pos, points_.size());
  points_.insert(points_.begin() + static_cast<int>(insert_pos), std::ref(obj_ref));
  update_indices();

  return true;
}

OptionalObserverPtr<SceneObject> PointList::point_first(const SceneObjectHandle& handle) {
  auto it = points_map_.find(handle);
  if (it == points_map_.end()) {
    return std::nullopt;
  }

  return OptionalObserverPtr<SceneObject>(points_[*it->second.begin()].get());
}

void PointList::update_indices() {
  for (auto& [key, val] : points_map_) {
    val.clear();
  }

  for (size_t i = points_.size(); i-- > 0;) {
    points_map_.at(points_[i].get().handle()).insert(i);
  }
}

std::expected<void, PointList::OperationError> PointList::replace(const SceneObject& old_point,
                                                                  SceneObject& new_point) {
  return replace(old_point.handle(), new_point);
}

std::expected<void, PointList::OperationError> PointList::replace(const SceneObjectHandle& old_point_handle,
                                                                  SceneObject& new_point) {
  if (!new_point.has_type<Point>()) {
    eray::util::Logger::err("Could not replace a point. New scene object is not a point.");
    return std::unexpected(OperationError::NotAPoint);
  }

  auto old_it = points_map_.find(old_point_handle);
  if (old_it == points_map_.end()) {
    eray::util::Logger::err("Could not replace a point. Old scene object not found.");
    return std::unexpected(OperationError::NotFound);
  }

  auto new_it = points_map_.find(new_point.handle());
  if (new_it == points_map_.end()) {
    auto [it, success] = points_map_.emplace(new_point.handle(), std::unordered_set<size_t>());
    if (!success) {
      eray::util::Logger::err("Could not replace a point. Map insertion failed.");
      return std::unexpected(OperationError::MapInsertionFailure);
    }
    new_it = it;
  }

  for (auto idx : old_it->second) {
    points_.at(idx) = new_point;
    new_it->second.insert(idx);
  }

  points_map_.erase(old_it);

  return {};
}

SceneObject& PointList::unsafe_by_idx(size_t idx) { return points_.at(idx).get(); }

const SceneObject& PointList::unsafe_by_idx(size_t idx) const { return points_.at(idx).get(); }

void PointList::push_back_many(Scene& scene, const std::vector<SceneObjectHandle>& handles) {
  for (const auto& h : handles) {
    if (auto o = scene.arena<SceneObject>().get_obj(h)) {
      auto& obj = *o.value();
      if (!obj.has_type<Point>()) {
        continue;
      }

      if (!push_back(obj)) {
        eray::util::Logger::err("Could not push back new scene object");
      }
    }
  }
}

void PointList::remove_many(const std::vector<SceneObjectHandle>& handles) {
  for (const auto& h : handles) {
    auto it = points_map_.find(h);
    if (it == points_map_.end()) {
      continue;
    }

    points_map_.erase(it);
  }

  int count = 0;
  for (auto i = 0U, j = 0U; i < points_.size(); ++i) {
    auto it = points_map_.find(points_[i].get().handle());
    if (it == points_map_.end()) {  // if invalid
      ++count;
    } else {
      points_[j++] = points_[i];
    }
  }
  points_.erase(points_.end() - count, points_.end());
}

}  // namespace mini
