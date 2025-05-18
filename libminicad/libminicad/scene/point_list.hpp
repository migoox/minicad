#pragma once

#include <expected>
#include <libminicad/scene/scene_object_handle.hpp>
#include <optional>
#include <ranges>

namespace mini {
class PointList {
 public:
  enum class OperationError : uint8_t {
    NotAPoint = 0,
    NotFound  = 1,
  };

  std::expected<void, OperationError> add(SceneObject& obj);
  void add_many(Scene& scene, const std::vector<SceneObjectHandle>& handles);
  std::expected<void, OperationError> remove(SceneObject& obj);
  void remove_many(const std::vector<SceneObjectHandle>& handles);
  std::expected<bool, OperationError> move_before(const SceneObjectHandle& dest, const SceneObjectHandle& obj);
  std::expected<bool, OperationError> move_after(const SceneObjectHandle& dest, const SceneObjectHandle& obj);

  void unsafe_set(Scene& scene, const std::vector<SceneObjectHandle>& handles);

  bool contains(const SceneObjectHandle& handle) { return points_map_.contains(handle); }

  void clear();

  OptionalObserverPtr<SceneObject> point(const SceneObjectHandle& handle);

  auto point_objects() {
    return points_ | std::ranges::views::transform([](auto& ref) -> auto& { return ref.get(); });
  }
  auto point_objects() const {
    return points_ | std::ranges::views::transform([](const auto& ref) -> const auto& { return ref.get(); });
  }

  auto point_handles() const { return points_map_ | std::views::keys; }
  auto point_handles() { return points_map_ | std::views::keys; }

  std::optional<size_t> point_idx(const SceneObjectHandle& handle) {
    auto it = points_map_.find(handle);
    if (it == points_map_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  SceneObject& unsafe_by_idx(size_t idx);
  const SceneObject& unsafe_by_idx(size_t idx) const;

  size_t size() const { return points_.size(); }

  std::vector<ref<SceneObject>>& unsafe_points() { return points_; }
  std::unordered_map<SceneObjectHandle, size_t>& unsafe_points_map() { return points_map_; }

 private:
  void update_indices_from(size_t start_idx);

 private:
  std::vector<ref<SceneObject>> points_;
  std::unordered_map<SceneObjectHandle, size_t> points_map_;
};

}  // namespace mini
