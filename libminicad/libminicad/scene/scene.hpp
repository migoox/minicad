#pragma once

#include <cstdint>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/arena.hpp>
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

  template <CObject TObject>
  std::expected<eray::util::Handle<TObject>, ObjectCreationError> create_obj(TObject::Variant&& variant) {
    auto h = arena<TObject>().create(*this, std::move(variant));
    if (!h) {
      return std::unexpected(ObjectCreationError::ReachedMaxObjects);
    }
    const auto& handle = *h;
    auto& obj          = arena<TObject>().unsafe_at(handle);
    obj.order_idx_     = objects_order_.size();
    objects_order_.emplace_back(handle);

    return handle;
  }

  template <CObject TObject>
  std::expected<std::vector<eray::util::Handle<TObject>>, ObjectCreationError> create_many_objs(
      TObject::Variant variant, size_t count) {
    auto h = arena<TObject>().create_many(*this, variant, count);
    if (!h) {
      return std::unexpected(ObjectCreationError::ReachedMaxObjects);
    }
    for (const auto& handle : *h) {
      auto& obj      = arena<TObject>().unsafe_at(handle);
      obj.order_idx_ = objects_order_.size();
      objects_order_.emplace_back(handle);
    }

    return *h;
  }

  template <CObject TObject>
  bool delete_obj(const eray::util::Handle<TObject>& handle) {
    if (auto o = arena<TObject>().get_obj(handle)) {
      remove_from_order(o.value()->order_idx_);
      arena<TObject>().delete_obj(handle);
      return true;
    }
    return false;
  }

  // TODO(migoox): delete many objs

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

  std::tuple<Arena<SceneObject>, Arena<Curve>, Arena<PatchSurface>> arenas_;

  std::vector<ObjectHandle> objects_order_;
};

}  // namespace mini
