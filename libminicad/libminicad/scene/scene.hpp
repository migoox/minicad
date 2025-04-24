#pragma once

#include <cstdint>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/ruleof.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <list>
#include <memory>
#include <optional>
#include <stack>
#include <variant>
#include <vector>

namespace mini {

template <typename T>
using ObserverPtr = eray::util::ObserverPtr<T>;

template <typename T>
using OptionalObserverPtr = std::optional<eray::util::ObserverPtr<T>>;

class Scene {
 public:
  Scene() = delete;
  explicit Scene(std::unique_ptr<ISceneRenderer>&& renderer);

  ERAY_ENABLE_DEFAULT_MOVE_AND_COPY_CTOR(Scene)
  ERAY_DISABLE_MOVE_AND_COPY_ASSIGN(Scene)

  enum class ObjectCreationError : uint8_t { ReachedMaxObjects = 0 };

  std::optional<SceneObjectHandle> handle_by_scene_obj_id(SceneObjectId id);
  std::optional<PointListObjectHandle> handle_by_point_list_obj_id(PointListObjectId id);

  [[nodiscard]] OptionalObserverPtr<SceneObject> get_obj(const SceneObjectHandle& handle);
  [[nodiscard]] OptionalObserverPtr<PointListObject> get_obj(const PointListObjectHandle& handle);

  template <typename Type>
  [[nodiscard]] OptionalObserverPtr<SceneObject> get_obj_that_holds(const SceneObjectHandle& handle) {
    if (!is_handle_valid(handle)) {
      return std::nullopt;
    }

    if (!std::holds_alternative<Type>(scene_objects_[handle.obj_id]->first->object)) {
      return std::nullopt;
    }

    return OptionalObserverPtr<SceneObject>(*scene_objects_[handle.obj_id]->first);
  }

  bool add_to_list(const SceneObjectHandle& p_handle, const PointListObjectHandle& pl_handle);
  bool remove_from_list(const SceneObjectHandle& p_handle, const PointListObjectHandle& pl_handle);

  [[nodiscard]] bool exists(const SceneObjectHandle& handle) const { return is_handle_valid(handle); }
  [[nodiscard]] bool exists(const PointListObjectHandle& handle) const { return is_handle_valid(handle); }

  bool delete_obj(const SceneObjectHandle& handle);
  bool delete_obj(const PointListObjectHandle& handle);

  std::expected<SceneObjectHandle, ObjectCreationError> create_scene_obj(SceneObjectVariant variant = Point{});
  std::expected<PointListObjectHandle, ObjectCreationError> create_list_obj(
      PointListObjectVariant variant = Polyline{});

  const std::list<SceneObjectHandle>& scene_objs() const { return scene_objects_list_; }
  const std::list<PointListObjectHandle>& point_list_objs() const { return point_list_objects_list_; }
  const std::vector<ObjectHandle>& objs() const { return objects_order_; }

  bool is_handle_valid(const PointListObjectHandle& handle) const;
  bool is_handle_valid(const SceneObjectHandle& handle) const;

  /**
   * @brief Calls renderer update, which fetches the commands from the queues and applies the changes
   * to the rendering state.
   *
   */
  void update_rendering_state() { renderer_->update(*this); }

  ISceneRenderer& renderer() { return *renderer_; }

 public:
  static constexpr std::size_t kMaxObjects = 100;

 private:
  SceneObjectHandle create_scene_obj_handle(std::uint32_t obj_id);
  PointListObjectHandle create_list_obj_handle(std::uint32_t obj_id);

  void remove_from_order(size_t ind);
  void add_to_order(SceneObject& obj);
  void add_to_order(PointListObject& obj);

  /**
   * @brief Returns an incrementing timestamp, wrapping around after UINT32_MAX.
   *
   * @return std::uint32_t
   */
  std::uint32_t timestamp();

 private:
  friend PointListObject;

  std::unique_ptr<ISceneRenderer> renderer_;

  static std::uint32_t next_signature_;

  std::uint32_t signature_;
  std::uint32_t curr_timestamp_;

  std::uint32_t curr_scene_obj_ind_;
  std::uint32_t curr_list_obj_ind_;

  std::list<SceneObjectHandle> scene_objects_list_;
  std::vector<std::optional<
      std::pair<std::unique_ptr<SceneObject>, std::pair<std::uint32_t, std::list<SceneObjectHandle>::iterator>>>>
      scene_objects_;
  std::stack<SceneObjectId> scene_objects_freed_;
  std::unordered_set<SceneObjectId> dirty_scene_objects_;

  std::list<PointListObjectHandle> point_list_objects_list_;
  std::vector<std::optional<std::pair<std::unique_ptr<PointListObject>,
                                      std::pair<std::uint32_t, std::list<PointListObjectHandle>::iterator>>>>
      point_list_objects_;
  std::stack<PointListObjectId> point_list_objects_freed_;
  std::unordered_set<PointListObjectId> dirty_point_list_objects_;

  std::vector<ObjectHandle> objects_order_;
};

}  // namespace mini
