#include <expected>
#include <libminicad/scene/point_list.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <limits>

namespace mini {

std::expected<void, PointList::OperationError> PointList::add(SceneObject& obj) {
  if (contains(obj.handle())) {
    return {};
  }

  if (obj.has_type<Point>()) {
    auto idx = points_.size();
    points_map_.emplace(obj.handle(), idx);
    points_.emplace_back(obj);
    return {};
  }

  return std::unexpected(OperationError::NotAPoint);
}

void PointList::unsafe_set(Scene& scene, const std::vector<SceneObjectHandle>& handles) {
  clear();
  for (auto i = 0U; const auto& handle : handles) {
    if (auto o = scene.arena<SceneObject>().get_obj(handle)) {
      auto& obj = *o.value();
      points_.emplace_back(obj);
      points_map_.emplace(obj.handle(), i);
      ++i;
    }
  }
}

void PointList::clear() {
  points_.clear();
  points_map_.clear();
}

std::expected<void, PointList::OperationError> PointList::remove(SceneObject& obj) {
  if (obj.has_type<Point>()) {
    auto it = points_map_.find(obj.handle());
    if (it != points_map_.end()) {
      auto idx = it->second;

      points_map_.erase(it);
      points_.erase(points_.begin() + static_cast<int>(idx));
      update_indices_from(idx);
    }

    return {};
  }

  return std::unexpected(OperationError::NotAPoint);
}

std::expected<bool, PointList::OperationError> PointList::move_before(const SceneObjectHandle& dest,
                                                                      const SceneObjectHandle& obj) {
  auto dest_it = points_map_.find(dest);
  auto obj_it  = points_map_.find(obj);

  if (dest_it != points_map_.end() && obj_it != points_map_.end()) {
    return std::unexpected(OperationError::NotAPoint);
  }

  size_t dest_idx = dest_it->second;
  size_t obj_idx  = obj_it->second;

  if (obj_idx == dest_idx) {
    return false;
  }
  SceneObject& obj_ref = points_[obj_idx].get();
  points_.erase(points_.begin() + static_cast<int>(obj_idx));
  if (obj_idx < dest_idx && dest_idx > 0) {
    dest_idx--;
  }
  points_.insert(points_.begin() + static_cast<int>(dest_idx), std::ref(obj_ref));
  update_indices_from(std::min(dest_idx, obj_idx));

  return true;
}

std::expected<bool, PointList::OperationError> PointList::move_after(const SceneObjectHandle& dest,
                                                                     const SceneObjectHandle& obj) {
  auto dest_it = points_map_.find(dest);
  auto obj_it  = points_map_.find(obj);

  if (dest_it != points_map_.end() && obj_it != points_map_.end()) {
    return std::unexpected(OperationError::NotAPoint);
  }

  size_t dest_idx = dest_it->second;
  size_t obj_idx  = obj_it->second;

  if (obj_idx == dest_idx + 1) {
    return false;
  }

  SceneObject& obj_ref = points_[obj_idx].get();
  points_.erase(points_.begin() + static_cast<int>(obj_idx));
  size_t insert_pos = dest_idx + 1;
  if (obj_idx > dest_idx && insert_pos > 0) {
    insert_pos--;
  }
  insert_pos = std::min(insert_pos, points_.size());
  points_.insert(points_.begin() + static_cast<int>(insert_pos), std::ref(obj_ref));
  update_indices_from(std::min(insert_pos, obj_idx));

  return true;
}

OptionalObserverPtr<SceneObject> PointList::point(const SceneObjectHandle& handle) {
  auto it = points_map_.find(handle);
  if (it == points_map_.end()) {
    return std::nullopt;
  }

  return OptionalObserverPtr<SceneObject>(points_[it->second].get());
}

void PointList::update_indices_from(size_t start_idx) {
  for (size_t i = start_idx; i < points_.size(); ++i) {
    points_map_[points_[i].get().handle()] = i;
  }
}

SceneObject& PointList::unsafe_by_idx(size_t idx) { return points_.at(idx).get(); }

const SceneObject& PointList::unsafe_by_idx(size_t idx) const { return points_.at(idx).get(); }

void PointList::add_many(Scene& scene, const std::vector<SceneObjectHandle>& handles) {
  for (auto i = 0U; const auto& h : handles) {
    if (contains(h)) {
      continue;
    }

    if (auto o = scene.arena<SceneObject>().get_obj(h)) {
      auto& obj = *o.value();
      if (!obj.has_type<Point>()) {
        continue;
      }

      points_.emplace_back(obj);
      points_map_.emplace(obj.handle(), i);
      ++i;
    }
  }
}

void PointList::remove_many(const std::vector<SceneObjectHandle>& handles) {
  int count = 0;
  for (const auto& h : handles) {
    if (!contains(h)) {
      continue;
    }
    ++count;
    points_map_[h] = std::numeric_limits<size_t>::max();  // mark as invalid
  }

  for (auto i = 0U, j = 0U; i < points_.size(); ++i) {
    if (points_map_[points_[i].get().handle()] != std::numeric_limits<size_t>::max()) {  // if valid
      points_[j++] = points_[i];
    }
  }

  for (auto i = 0U; i < points_.size(); ++i) {
    if (points_map_[points_[i].get().handle()] != std::numeric_limits<size_t>::max()) {
      points_map_.erase(points_[i].get().handle());
    } else {
      points_map_.at(points_[i].get().handle()) = i;
    }
  }
  points_.erase(points_.end() - count, points_.end());
}

}  // namespace mini
