#pragma once

#include <cstdint>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/arena.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <memory>
#include <vector>

namespace mini {

class Scene {
 public:
  Scene() = delete;
  explicit Scene(std::unique_ptr<ISceneRenderer>&& renderer);

  Scene(Scene&&) noexcept      = default;
  Scene(const Scene&) noexcept = delete;
  ERAY_DELETE_MOVE_AND_COPY_ASSIGN(Scene)

  enum class ObjectCreationError : uint8_t { ReachedMaxObjects = 0 };

  template <typename TObject>
  Arena<TObject>& arena() {
    return std::get<Arena<TObject>>(arenas_);
  }

  template <typename TObject>
  const Arena<TObject>& arena() const {
    return std::get<Arena<TObject>>(arenas_);
  }

  bool add_point_to_curve(const SceneObjectHandle& p_handle, const CurveHandle& c_handle);
  bool remove_point_from_curve(const SceneObjectHandle& p_handle, const CurveHandle& c_handle);

  std::expected<SceneObjectHandle, ObjectCreationError> create_scene_obj(SceneObjectVariant variant = Point{});
  std::expected<CurveHandle, ObjectCreationError> create_curve_obj(CurveVariant variant = Polyline{});

  template <typename THandle>
  bool delete_obj(const THandle handle) {
    using Object = typename THandle::Object;

    if (auto o = arena<Object>().get_obj(handle)) {
      remove_from_order(o.value()->order_idx_);
      arena<Object>().delete_obj(handle);
      return true;
    }
    return false;
  }

  const std::vector<ObjectHandle>& objs() const { return objects_order_; }

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
  void remove_from_order(size_t ind);
  void add_to_order(const ObjectHandle& obj);

 private:
  friend Curve;

  std::unique_ptr<ISceneRenderer> renderer_;

  static std::uint32_t next_signature_;
  std::uint32_t signature_;

  std::tuple<Arena<SceneObject>, Arena<Curve>> arenas_;

  std::vector<ObjectHandle> objects_order_;
};

}  // namespace mini
