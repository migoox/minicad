#pragma once

#include <concepts>
#include <generator>
#include <liberay/math/transform3.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/variant_match.hpp>
#include <liberay/util/zstring_view.hpp>
#include <libminicad/scene/point_list.hpp>
#include <libminicad/scene/handles.hpp>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <variant>

namespace mini {

// ---------------------------------------------------------------------------------------------------------------------
// - ObjectBase --------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
concept CObject =
    requires(T t, T obj, const eray::util::Handle<T>& handle, Scene& scene, std::string&& name, std::size_t idx) {
      typename T::Variant;

      T{handle, scene};
      { t.order_idx() } -> std::same_as<std::size_t>;
      { t.handle() } -> std::same_as<const eray::util::Handle<T>&>;
      { t.id() } -> std::same_as<typename eray::util::Handle<T>::ObjectId>;
      { t.scene() } -> std::same_as<Scene&>;
      { std::as_const(t).scene() } -> std::same_as<const Scene&>;
      { t.can_be_deleted() } -> std::same_as<bool>;
      { t.on_delete() } -> std::same_as<void>;  // TODO(migoox): find better solution
      { t.type_name() } -> std::same_as<zstring_view>;
      { t.set_name(std::move(name)) } -> std::same_as<void>;
      { std::as_const(t).clone_to(obj) } -> std::same_as<void>;
    };

template <typename TObject, typename TVariant>
class ObjectBase {
 public:
  MINI_VALIDATE_VARIANT_TYPES(TVariant, CObjectVariant);
  using Variant = TVariant;

  size_t order_idx() const { return order_idx_; }
  const eray::util::Handle<TObject>& handle() const { return handle_; }
  typename eray::util::Handle<TObject>::ObjectId id() const { return handle_.obj_id; }

  Scene& scene() { return scene_.get(); }
  const Scene& scene() const { return scene_.get(); }

  template <typename Type>
    requires std::is_constructible_v<TVariant, Type>
  bool has_type() const {
    return std::holds_alternative<Type>(object);
  }

  /**
   * @brief If the SceneObject is not of the requested Type, this function will fail. Use with caution.
   *
   * @tparam Type
   * @return requires&
   */
  template <typename Type>
    requires std::is_constructible_v<TVariant, Type>
  Type& unsafe_get_variant() {
    return std::get<Type>(object);
  }

  zstring_view type_name() {
    return std::visit(eray::util::match{[&](const auto& o) {
                        using T = std::decay_t<decltype(o)>;
                        return T::type_name();
                      }},
                      object);
  }

  void set_name(std::string&& new_name) { name = std::move(new_name); }

 public:
  TVariant object;
  std::string name;

 protected:
  friend Scene;

  eray::util::Handle<TObject> handle_;
  std::size_t order_idx_{0};

  mutable ref<Scene> scene_;  // IMPORTANT: lifetime of the scene always exceeds the lifetime of the scene object
 private:
  friend TObject;

  ObjectBase(const eray::util::Handle<TObject>& handle, Scene& scene) : handle_(handle), scene_(scene) {}
};

// ---------------------------------------------------------------------------------------------------------------------
// - PointListObject ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

template <typename TObject>
class PointListObjectBase {
 public:
  auto point_objects() { return points_.point_objects(); }
  auto point_objects() const { return points_.point_objects(); }
  auto point_handles() const { return points_.point_handles(); }

  auto points() const {
    return point_objects() | std::views::transform([](const auto& s) { return s.transform.pos(); });
  }

  bool contains(const SceneObjectHandle& handle) const { return points_.contains(handle); }

  OptionalObserverPtr<SceneObject> point(const SceneObjectHandle& handle) { return points_.point_first(handle); }

  std::optional<size_t> point_idx(const SceneObjectHandle& handle) const { return points_.point_first_idx(handle); }

  size_t points_count() const { return points_.size(); }

 protected:
  PointList points_;

 private:
  friend Curve;
  friend PatchSurface;

  PointListObjectBase() = default;
};

// ---------------------------------------------------------------------------------------------------------------------
// - SceneObjectType ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class Point {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Point"; }
};

class Torus {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Torus"; }

 public:
  float minor_radius           = 1.F;
  float major_radius           = 2.F;
  eray::math::Vec2i tess_level = eray::math::Vec2i(16, 16);
};

template <typename T>
concept CSceneObjectType = requires {
  { T::type_name() } -> std::same_as<zstring_view>;
};

using SceneObjectVariant = std::variant<Point, Torus>;
MINI_VALIDATE_VARIANT_TYPES(SceneObjectVariant, CSceneObjectType);

// ---------------------------------------------------------------------------------------------------------------------
// - SceneObject -------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class SceneObject : public ObjectBase<SceneObject, SceneObjectVariant> {
 public:
  SceneObject() = delete;
  SceneObject(SceneObjectHandle handle, Scene& scene);

  ERAY_DEFAULT_MOVE(SceneObject)
  ERAY_DELETE_COPY(SceneObject)

  void update();
  void on_delete();
  bool can_be_deleted() const;

  void clone_to(SceneObject& obj) const;

 public:
  eray::math::Transform3f transform;

 private:
  friend Scene;
  friend Curve;
  friend PatchSurface;
  friend FillInSurface;

  void move_refs_to(SceneObject& obj);

 private:
  std::unordered_set<CurveHandle> curves_;
  std::unordered_set<PatchSurfaceHandle> patch_surfaces_;
  std::unordered_set<FillInSurfaceHandle> fill_in_surfaces_;
};

}  // namespace mini
