#pragma once

#include <expected>
#include <libminicad/scene/scene_object_handle.hpp>
#include <optional>
#include <ranges>
#include <unordered_set>

namespace mini {

/**
 * @brief Maintains a list of point references. The references might repeat.
 *
 */
class PointList {
 public:
  enum class OperationError : uint8_t {
    NotAPoint           = 0,
    NotFound            = 1,
    MapInsertionFailure = 2,
  };

  std::expected<void, OperationError> push_back(SceneObject& obj);
  void push_back_many(Scene& scene, const std::vector<SceneObjectHandle>& handles);

  /**
   * @brief Remove all references to the given point.
   *
   * @param obj
   * @return std::expected<void, OperationError>
   */
  std::expected<void, OperationError> remove(SceneObject& obj);

  /**
   * @brief Remove all references to the points with provided handles.
   *
   * @param handles
   */
  void remove_many(const std::vector<SceneObjectHandle>& handles);

  std::expected<void, OperationError> replace(const SceneObjectHandle& old_point_handle, SceneObject& new_point);
  std::expected<void, OperationError> replace(const SceneObject& old_point, SceneObject& new_point);

  std::expected<bool, OperationError> move_before(size_t dest_idx, size_t source_idx);
  std::expected<bool, OperationError> move_after(size_t dest_idx, size_t source_idx);

  void unsafe_set(Scene& scene, const std::vector<SceneObjectHandle>& handles);

  bool contains(const SceneObjectHandle& handle) const { return points_map_.contains(handle); }

  void clear();

  /**
   * @brief Returns first point with the given handle.
   *
   * @param handle
   * @return OptionalObserverPtr<SceneObject>
   */
  OptionalObserverPtr<SceneObject> point_first(const SceneObjectHandle& handle);

  auto point_objects() {
    return points_ | std::ranges::views::transform([](auto& ref) -> auto& { return ref.get(); });
  }
  auto point_objects() const {
    return points_ | std::ranges::views::transform([](const auto& ref) -> const auto& { return ref.get(); });
  }

  auto point_handles() const {
    return points_ |
           std::views::transform([](const auto& obj) -> const SceneObjectHandle& { return obj.get().handle(); });
  }

  auto unique_point_handles() const { return points_map_ | std::views::keys; }

  std::optional<size_t> point_first_idx(const SceneObjectHandle& handle) const {
    auto it = points_map_.find(handle);
    if (it == points_map_.end()) {
      return std::nullopt;
    }
    return *it->second.begin();  // there is always at least one element in this set
  }

  [[nodiscard]] SceneObject& unsafe_by_idx(size_t idx);
  [[nodiscard]] const SceneObject& unsafe_by_idx(size_t idx) const;

  size_t size() const { return points_.size(); }
  size_t size_unique() const { return points_map_.size(); }

  std::vector<ref<SceneObject>>& unsafe_points() { return points_; }
  std::unordered_map<SceneObjectHandle, std::unordered_set<size_t>>& unsafe_points_map() { return points_map_; }

 private:
  void update_indices();

 private:
  std::vector<ref<SceneObject>> points_;
  std::unordered_map<SceneObjectHandle, std::unordered_set<size_t>> points_map_;
};

}  // namespace mini
