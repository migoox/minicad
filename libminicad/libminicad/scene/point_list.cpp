#include <expected>
#include <libminicad/scene/point_list.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <limits>

namespace mini {

std::expected<void, PointList::OperationError> PointList::push_back(SceneObject& obj) {
  if (!obj.has_type<Point>()) {
    return std::unexpected(OperationError::NotAPoint);
  }

  if (contains(obj.handle())) {
    points_.emplace_back(obj);
  } else {
    auto idx = points_.size();
    points_map_.emplace(obj.handle(), idx);
    points_.emplace_back(obj);
  }

  return {};
}

void PointList::unsafe_set(Scene& scene, const std::vector<SceneObjectHandle>& handles) {
  clear();
  for (auto idx = 0U; const auto& handle : handles) {
    auto& obj = *scene.arena<SceneObject>().unsafe_get_obj(handle);

    points_.emplace_back(obj);
    if (!points_map_.contains(handle)) {
      points_map_.emplace(handle, idx);
    }

    ++idx;
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

OptionalObserverPtr<SceneObject> PointList::point(const SceneObjectHandle& handle) {
  auto it = points_map_.find(handle);
  if (it == points_map_.end()) {
    return std::nullopt;
  }

  return OptionalObserverPtr<SceneObject>(points_[it->second].get());
}

void PointList::update_indices() {
  for (auto i = points_.size() - 1; i > 0; --i) {
    points_map_.at(points_[i].get().handle()) = i;
  }
  if (!points_.empty()) {
    points_map_.at(points_[0].get().handle()) = 0;
  }
}

SceneObject& PointList::unsafe_by_idx(size_t idx) { return points_.at(idx).get(); }

const SceneObject& PointList::unsafe_by_idx(size_t idx) const { return points_.at(idx).get(); }

void PointList::push_back_many(Scene& scene, const std::vector<SceneObjectHandle>& handles) {
  for (auto i = 0U; const auto& h : handles) {
    if (auto o = scene.arena<SceneObject>().get_obj(h)) {
      auto& obj = *o.value();
      if (!obj.has_type<Point>()) {
        continue;
      }

      points_.emplace_back(obj);
      if (!contains(h)) {
        points_map_.emplace(obj.handle(), i);
      }
      ++i;
    }
  }
}

void PointList::remove_many(const std::vector<SceneObjectHandle>& handles) {
  for (const auto& h : handles) {
    if (!contains(h)) {
      continue;
    }
    points_map_[h] = std::numeric_limits<size_t>::max();  // mark as invalid
  }

  int count = 0;
  for (auto i = 0U, j = 0U; i < points_.size(); ++i) {
    if (points_map_[points_[i].get().handle()] != std::numeric_limits<size_t>::max()) {  // if valid
      points_[j++] = points_[i];
    } else {
      ++count;
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
