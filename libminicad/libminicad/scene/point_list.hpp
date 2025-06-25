#pragma once

#include <expected>
#include <libminicad/scene/handles.hpp>
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

  std::expected<void, OperationError> push_back(PointObject& obj);
  void push_back_many(Scene& scene, const std::vector<PointObjectHandle>& handles);

  /**
   * @brief Remove all references to the given point.
   *
   * @param obj
   * @return std::expected<void, OperationError>
   */
  std::expected<void, OperationError> remove(PointObject& obj);

  /**
   * @brief Remove all references to the points with provided handles.
   *
   * @param handles
   */
  void remove_many(const std::vector<PointObjectHandle>& handles);

  std::expected<void, OperationError> replace(const PointObjectHandle& old_point_handle, PointObject& new_point);
  std::expected<void, OperationError> replace(const PointObject& old_point, PointObject& new_point);

  std::expected<bool, OperationError> move_before(size_t dest_idx, size_t source_idx);
  std::expected<bool, OperationError> move_after(size_t dest_idx, size_t source_idx);

  void unsafe_set(Scene& scene, const std::vector<PointObjectHandle>& handles);

  bool contains(const PointObjectHandle& handle) const { return points_map_.contains(handle); }

  void clear();

  /**
   * @brief Returns first point with the given handle.
   *
   * @param handle
   * @return OptionalObserverPtr<SceneObject>
   */
  OptionalObserverPtr<PointObject> point_first(const PointObjectHandle& handle);

  auto point_objects() {
    return points_ | std::ranges::views::transform([](auto& ref) -> auto& { return ref.get(); });
  }
  auto point_objects() const {
    return points_ | std::ranges::views::transform([](const auto& ref) -> const auto& { return ref.get(); });
  }

  auto point_handles() const {
    return points_ |
           std::views::transform([](const auto& obj) -> const PointObjectHandle& { return obj.get().handle(); });
  }

  auto unique_point_handles() const { return points_map_ | std::views::keys; }

  std::optional<size_t> point_first_idx(const PointObjectHandle& handle) const {
    auto it = points_map_.find(handle);
    if (it == points_map_.end()) {
      return std::nullopt;
    }
    return *it->second.begin();  // there is always at least one element in this set
  }

  [[nodiscard]] PointObject& unsafe_by_idx(size_t idx);
  [[nodiscard]] const PointObject& unsafe_by_idx(size_t idx) const;

  size_t size() const { return points_.size(); }
  size_t size_unique() const { return points_map_.size(); }

  std::vector<ref<PointObject>>& unsafe_points() { return points_; }
  std::unordered_map<PointObjectHandle, std::unordered_set<size_t>>& unsafe_points_map() { return points_map_; }

 private:
  void update_indices();

 private:
  std::vector<ref<PointObject>> points_;
  std::unordered_map<PointObjectHandle, std::unordered_set<size_t>> points_map_;
};

}  // namespace mini
