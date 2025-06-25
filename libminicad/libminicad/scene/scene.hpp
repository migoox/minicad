#pragma once

#include <cstdint>
#include <expected>
#include <iterator>
#include <liberay/util/iterator.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/approx_curve.hpp>
#include <libminicad/scene/arena.hpp>
#include <libminicad/scene/curve.hpp>
#include <libminicad/scene/fill_in_suface.hpp>
#include <libminicad/scene/handles.hpp>
#include <libminicad/scene/param_primitive.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/triangle.hpp>
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

  enum class ObjectCreationError : uint8_t {
    ReachedMaxObjects = 0,
    CopyFailure       = 1,
  };
  enum class PointsMergeError : uint8_t { NotEnoughPoints = 0, NewPointCreationError = 1 };

  template <typename TObject>
  [[nodiscard]] Arena<TObject>& arena() {
    return std::get<Arena<TObject>>(arenas_);
  }

  template <typename TObject>
  [[nodiscard]] const Arena<TObject>& arena() const {
    return std::get<Arena<TObject>>(arenas_);
  }

  bool push_back_point_to_curve(const PointObjectHandle& p_handle, const CurveHandle& c_handle);
  bool remove_point_from_curve(const PointObjectHandle& p_handle, const CurveHandle& c_handle);

  /**
   * @brief Replaces all provided points with it's mean point. All the references are updated.
   *
   * @tparam It
   * @param begin
   * @param end
   */
  template <eray::util::Iterator<PointObjectHandle> It>
  std::expected<eray::util::Handle<PointObject>, PointsMergeError> merge_points(const It& begin, const It& end) {
    auto count = std::ranges::distance(begin, end);
    if (count <= 1) {
      eray::util::Logger::warn("Could not merge points. Not enough points provided.");
      return std::unexpected(PointsMergeError::NotEnoughPoints);
    }

    auto new_pos = eray::math::Vec3f::filled(0.F);

    auto opt = create_obj_and_get<PointObject>(Point{});
    if (!opt) {
      eray::util::Logger::err("Could not merge points. New point could not be created.");
      return std::unexpected(PointsMergeError::NewPointCreationError);
    }
    auto& obj = **opt;

    for (const auto& handle : std::ranges::subrange(begin, end)) {
      if (auto opt_old = arena<PointObject>().get_obj(handle)) {
        auto& obj_old = **opt_old;
        if (obj_old.template has_type<Point>()) {
          obj_old.move_refs_to(obj);
          new_pos += obj_old.transform().pos();
          remove_from_order(obj_old.order_idx_);
          arena<PointObject>().delete_obj(handle);
        }
      }
    }

    new_pos = new_pos / static_cast<float>(count);
    obj.transform().set_local_pos(new_pos);
    obj.update();

    return obj.handle();
  }

  template <CObject TObject>
  std::expected<ObserverPtr<TObject>, ObjectCreationError> create_obj_and_get(TObject::Variant&& variant) {
    auto obj = arena<TObject>().create_and_get(*this, std::move(variant));
    if (!obj) {
      return std::unexpected(ObjectCreationError::ReachedMaxObjects);
    }
    auto handle             = obj.value()->handle();
    obj.value()->order_idx_ = objects_order_.size();
    objects_order_.emplace_back(handle);

    return std::move(*obj);
  }

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
  std::expected<eray::util::Handle<TObject>, ObjectCreationError> clone_obj(const eray::util::Handle<TObject>& handle) {
    if (auto o = arena<TObject>().get_obj(handle)) {
      if (auto o2 = create_obj_and_get<TObject>(decltype(o.value()->object){})) {
        auto& obj = **o2;
        o.value()->clone_to(obj);
        return obj.handle_;
      }
    }
    eray::util::Logger::err("Object copy failure");
    return std::unexpected(Scene::ObjectCreationError::CopyFailure);
  }

  template <CObject TObject>
  bool delete_obj(const eray::util::Handle<TObject>& handle) {
    if (auto o = arena<TObject>().get_obj(handle)) {
      if (arena<TObject>().delete_obj(handle)) {
        remove_from_order(o.value()->order_idx_);
      }
      return true;
    }
    return false;
  }

  bool fill_in_surface_exists(const Triangle& triangle) { return fill_in_surface_triangles_.contains(triangle); }

  // TODO(migoox): delete many objs

  const std::vector<ObjectHandle>& handles() const { return objects_order_; }

  /**
   * @brief Calls renderer update, which fetches the commands from the queues and applies the changes
   * to the rendering state.
   *
   */
  void update_rendering_state() { renderer_->update(*this); }

  ISceneRenderer& renderer() { return *renderer_; }

  void clear();

 public:
  static constexpr std::size_t kMaxObjects = 10000;

 private:
  void remove_from_order(size_t ind);
  void add_to_order(const ObjectHandle& obj);

 private:
  friend Curve;
  friend FillInSurface;

  std::unique_ptr<ISceneRenderer> renderer_;

  static std::uint32_t next_signature_;
  std::uint32_t signature_;

  std::tuple<Arena<PointObject>, Arena<Curve>, Arena<PatchSurface>, Arena<FillInSurface>, Arena<ApproxCurve>,
             Arena<ParamPrimitive>>
      arenas_;

  std::vector<ObjectHandle> objects_order_;

  std::unordered_set<Triangle> fill_in_surface_triangles_;
};

}  // namespace mini
