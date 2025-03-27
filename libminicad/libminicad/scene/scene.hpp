#pragma once

#include <cstdint>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/ruleof.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <memory>
#include <stack>
#include <vector>

namespace mini {

template <typename T>
using ObserverPtr = eray::util::ObserverPtr<T>;

template <typename T>
using OptionalObserverPtr = std::optional<eray::util::ObserverPtr<T>>;

class Scene {
 public:
  Scene();

  ERAY_DISABLE_MOVE_AND_COPY_ASSIGN(Scene)

  enum class ObjectCreationError : uint8_t { ReachedMaxObjects = 0 };

  [[nodiscard]] OptionalObserverPtr<SceneObject> get_obj(const SceneObjectHandle& handle);
  [[nodiscard]] OptionalObserverPtr<PointListObject> get_obj(const PointListObjectHandle& handle);

  [[nodiscard]] bool exists(const SceneObjectHandle& handle) { return is_handle_valid(handle); }
  [[nodiscard]] bool exists(const PointListObjectHandle& handle) { return is_handle_valid(handle); }

  bool delete_obj(const SceneObjectHandle& handle);
  bool delete_obj(const PointListObjectHandle& handle);

  [[nodiscard]] std::expected<SceneObjectHandle, ObjectCreationError> create_scene_obj();
  [[nodiscard]] std::expected<PointListObjectHandle, ObjectCreationError> create_list_obj();

  [[nodiscard]] std::optional<PointHandle> get_point_handle(const SceneObjectHandle& handle);
  bool add_point_to_list(const PointHandle& p_handle, const PointListObjectHandle& pl_handle);
  bool remove_point_from_list(const PointHandle& p_handle, const PointListObjectHandle& pl_handle);

  auto scene_obj_begin() const { return scene_objects_.begin(); }
  auto scene_obj_begin() { return scene_objects_.begin(); }
  auto scene_obj_end() const { return scene_objects_.end(); }
  auto scene_obj_end() { return scene_objects_.end(); }

  auto list_obj_begin() const { return point_list_objects_.begin(); }
  auto list_obj_begin() { return point_list_objects_.begin(); }
  auto list_obj_end() const { return point_list_objects_.end(); }
  auto list_obj_end() { return point_list_objects_.end(); }

 private:
  bool is_handle_valid(const PointListObjectHandle& handle);
  bool is_handle_valid(const SceneObjectHandle& handle);
  bool is_handle_valid(const PointHandle& handle);

  SceneObjectHandle create_scene_obj_handle(std::uint32_t obj_id);
  PointListObjectHandle create_list_obj_handle(std::uint32_t obj_id);

  /**
   * @brief Returns an incrementing timestamp, wrapping around after UINT32_MAX.
   *
   * @return std::uint32_t
   */
  std::uint32_t timestamp();

 private:
  static constexpr std::size_t kMaxSceneObjects = 1000;
  static std::uint32_t next_signature_;

  const std::uint32_t signature_;
  std::uint32_t curr_timestamp_;

  std::vector<std::optional<std::pair<std::unique_ptr<SceneObject>, std::uint32_t>>> scene_objects_;
  std::stack<uint32_t> scene_objects_freed_;

  std::vector<std::optional<std::pair<std::unique_ptr<PointListObject>, std::uint32_t>>> point_list_objects_;
  std::stack<uint32_t> point_list_objects_freed_;
};

}  // namespace mini
