#pragma once

#include <concepts>
#include <cstdint>
#include <generator>
#include <liberay/math/transform3.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/zstring_view.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace mini {

// ---------------------------------------------------------------------------------------------------------------------
// - ObjectBase --------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
concept CObject = requires(T t, const eray::util::Handle<T>& handle, Scene& scene) {
  T{handle, scene};
  { t.order_idx() } -> std::same_as<std::size_t>;
  { t.handle() } -> std::same_as<const eray::util::Handle<T>&>;
  { t.id() } -> std::same_as<typename eray::util::Handle<T>::ObjectId>;
  { t.scene() } -> std::same_as<Scene&>;
  { std::as_const(t).scene() } -> std::same_as<const Scene&>;
  { t.on_delete() } -> std::same_as<void>;  // TODO(migoox): find better solution
};

template <typename TObject, typename TVariant>
class ObjectBase {
 public:
  size_t order_idx() const { return order_idx_; }
  const eray::util::Handle<TObject>& handle() const { return handle_; }
  typename eray::util::Handle<TObject>::ObjectId id() const { return handle_.obj_id; }

  Scene& scene() { return scene_.get(); }
  const Scene& scene() const { return scene_.get(); }

  template <typename Type>
    requires std::is_constructible_v<TVariant, Type>
  bool has_type() {
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
  Type& get_variant() {
    assert(std::holds_alternative<Type>(object) && "Curve is not of the requested type.");
    return std::get<Type>(object);
  }

 public:
  TVariant object;
  std::string name;

 protected:
  friend Scene;

  eray::util::Handle<TObject> handle_;
  std::size_t order_idx_{0};

  ref<Scene> scene_;  // IMPORTANT: lifetime of the scene always exceeds the lifetime of the scene object
 private:
  friend SceneObject;
  friend Curve;
  friend PatchSurface;

  ObjectBase(const eray::util::Handle<TObject>& handle, Scene& scene) : handle_(handle), scene_(scene) {}
};

// ---------------------------------------------------------------------------------------------------------------------
// - PointListObject ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

template <typename TObject>
class PointListObjectBase {
 public:
  auto point_objects() {
    return points_ | std::ranges::views::transform([](auto& ref) -> auto& { return ref.get(); });
  }

  auto point_objects() const {
    return points_ | std::ranges::views::transform([](const auto& ref) -> const auto& { return ref.get(); });
  }

  auto point_handles() const { return points_map_ | std::views::keys; }

  auto points() const {
    return point_objects() | std::views::transform([](const auto& s) { return s.transform.pos(); });
  }

  bool contains(const SceneObjectHandle& handle) { return points_map_.contains(handle); }

  OptionalObserverPtr<SceneObject> point(const SceneObjectHandle& handle) {
    if (!contains(handle)) {
      return std::nullopt;
    }

    return OptionalObserverPtr<SceneObject>(points_.at(points_map_.at(handle)).get());
  }

  std::optional<size_t> point_idx(const SceneObjectHandle& handle) {
    if (!contains(handle)) {
      return std::nullopt;
    }

    return points_map_.at(handle);
  }

 protected:
  std::vector<ref<SceneObject>> points_;
  std::unordered_map<SceneObjectHandle, size_t> points_map_;

 private:
  friend Curve;
  friend PatchSurface;

  PointListObjectBase() = default;
};

// ---------------------------------------------------------------------------------------------------------------------
// - SceneObjectType ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
concept CSceneObjectType = requires {
  { T::type_name() } -> std::same_as<zstring_view>;
};

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

using SceneObjectVariant = std::variant<Point, Torus>;

// Check if all of the variant types are valid
template <typename Variant>
struct ValidateSceneObjectVariantTypes;
template <typename... Types>
struct ValidateSceneObjectVariantTypes<std::variant<Types...>> {
  static constexpr bool kAreVariantTypesValid = (CSceneObjectType<Types> && ...);
};
static_assert(ValidateSceneObjectVariantTypes<SceneObjectVariant>::kAreVariantTypesValid,
              "Not all variant types satisfy CSceneObjectType");

// ---------------------------------------------------------------------------------------------------------------------
// - SceneObject -------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class SceneObject : public ObjectBase<SceneObject, SceneObjectVariant> {
 public:
  SceneObject() = delete;
  ERAY_DEFAULT_MOVE(SceneObject)
  ERAY_DELETE_COPY(SceneObject)
  SceneObject(SceneObjectHandle handle, Scene& scene) : ObjectBase<SceneObject, SceneObjectVariant>(handle, scene) {}

  void update();
  void on_delete();

 public:
  eray::math::Transform3f transform;

 private:
  friend Scene;
  friend Curve;

 private:
  std::unordered_set<CurveHandle> curves_;
  std::unordered_set<PatchSurfaceHandle> patch_surfaces_;
};

// ---------------------------------------------------------------------------------------------------------------------
// - CurveType -----------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
concept CCurveType =
    requires(T t, Curve& base, ref<const Curve> base_ref, const SceneObject& point_obj, const Point& point) {
      { T::type_name() } -> std::same_as<zstring_view>;
      { t.on_point_update(base, point_obj, point) } -> std::same_as<void>;
      { t.on_point_add(base, point_obj, point) } -> std::same_as<void>;
      { t.on_point_remove(base, point_obj, point) } -> std::same_as<void>;
      { t.on_curve_reorder(base) } -> std::same_as<void>;
      { t.bezier3_points(base_ref) } -> std::same_as<std::generator<eray::math::Vec3f>>;
      { t.bezier3_points_count(base_ref) } -> std::same_as<size_t>;
    };

struct Polyline {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Polyline"; }
  void on_point_update(Curve&, const SceneObject&, const Point&) {}
  void on_point_add(Curve&, const SceneObject&, const Point&) {}
  void on_point_remove(Curve&, const SceneObject&, const Point&) {}
  void on_curve_reorder(Curve&) {}
  std::generator<eray::math::Vec3f> bezier3_points(ref<const Curve> base) const;
  size_t bezier3_points_count(ref<const Curve> base) const;
};

struct MultisegmentBezierCurve {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Multisegment C0 Bezier Curve"; }
  void on_point_update(Curve&, const SceneObject&, const Point&) {}
  void on_point_add(Curve&, const SceneObject&, const Point&) {}
  void on_point_remove(Curve&, const SceneObject&, const Point&) {}
  void on_curve_reorder(Curve&) {}
  std::generator<eray::math::Vec3f> bezier3_points(ref<const Curve> base) const;
  size_t bezier3_points_count(ref<const Curve> base) const;
};

/**
 * @brief The scene point objects represent the de Boor points of the BSplineCurve.
 *
 */
struct BSplineCurve {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "B-Spline Curve"; }

  /**
   * @brief Resets the bernstein points basing on the de Boor points (scene point objects).
   *
   */
  void reset_bernstein_points(const Curve& base);

  /**
   * @brief Updates the de Boor points (scene point objects) basing on the de Boor points.
   *
   */
  void update_de_boor_points(Curve& base);

  /**
   * @brief Updates the Bernstein points (scene point objects) basing on the de Boor points.
   *
   */
  void update_bernstein_points(Curve& base, const SceneObjectHandle& handle);

  const std::vector<eray::math::Vec3f>& bernstein_points() const { return bezier_points_; }

  std::optional<eray::math::Vec3f> bernstein_point(size_t idx) const {
    if (bezier_points_.size() > idx) {
      return bezier_points_[idx];
    }

    return std::nullopt;
  }

  void set_bernstein_point(Curve& base, size_t idx, const eray::math::Vec3f& point);

  bool contains(size_t idx) { return bezier_points_.size() > idx; }

  void on_point_update(Curve& base, const SceneObject&, const Point&);
  void on_point_add(Curve& base, const SceneObject&, const Point&);
  void on_point_remove(Curve& base, const SceneObject&, const Point&);
  void on_curve_reorder(Curve& base);
  std::generator<eray::math::Vec3f> bezier3_points(ref<const Curve> base) const;
  std::generator<eray::math::Vec3f> unique_bezier3_points(ref<const Curve> base) const;
  size_t bezier3_points_count(ref<const Curve> base) const;
  size_t unique_bezier3_points_count(ref<const Curve> base) const;

 private:
  /**
   * @brief Updates the bernstein points basing on the de Boor points (scene point objects). Invoked automatically
   * when the de Boor points are marked dirty.
   *
   * @param base
   * @param idx: represents first control point (de boor point) index in the segment of length 4
   */
  void update_bernstein_segment(const Curve& base, int cp_idx);

 private:
  std::vector<eray::math::Vec3f> bezier_points_;
};

class NaturalSplineCurve {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Natural Spline Curve"; }

  /**
   * @brief Describes one segment in the natural spline using a power basis.
   *
   */
  struct Segment {
    eray::math::Vec3f a, b, c, d;
    float chord_length = 1.F;

    static constexpr float kLengthEpsilon = 0.0001F;
  };

  /**
   * @brief Reset segments basing on all interpolating points.
   *
   */
  void reset_segments(Curve& base);

  const std::vector<Segment>& segments() const { return segments_; }
  const std::vector<eray::math::Vec3f>& unique_points() const { return unique_points_; }

  void on_point_update(Curve& base, const SceneObject&, const Point&) { update(base); }
  void on_point_add(Curve& base, const SceneObject&, const Point&) { update(base); }
  void on_point_remove(Curve& base, const SceneObject&, const Point&) { update(base); }
  void on_curve_reorder(Curve& base) { update(base); }
  std::generator<eray::math::Vec3f> bezier3_points(ref<const Curve> base) const;
  size_t bezier3_points_count(ref<const Curve> base) const;

 private:
  void update(Curve& base);

 private:
  std::vector<Segment> segments_;
  std::vector<eray::math::Vec3f> unique_points_;
};

using CurveVariant = std::variant<Polyline, MultisegmentBezierCurve, BSplineCurve, NaturalSplineCurve>;

// Check if all of the variant types are valid
template <typename Variant>
struct ValidateCurveVariantTypes;
template <typename... Types>
struct ValidateCurveVariantTypes<std::variant<Types...>> {
  static constexpr bool kAreVariantTypesValid = (CCurveType<Types> && ...);
};
static_assert(ValidateCurveVariantTypes<CurveVariant>::kAreVariantTypesValid,
              "Not all variant types satisfy CCurveType");

// ---------------------------------------------------------------------------------------------------------------------
// - Curve ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class Curve : public ObjectBase<Curve, CurveVariant>, public PointListObjectBase<Curve> {
 public:
  Curve() = delete;
  Curve(const CurveHandle& handle, Scene& scene) : ObjectBase<Curve, CurveVariant>(handle, scene) {}
  ERAY_DEFAULT_MOVE(Curve)
  ERAY_DELETE_COPY(Curve)

  std::generator<eray::math::Vec3f> polyline_points() const;
  size_t polyline_points_count() const;

  std::generator<eray::math::Vec3f> bezier3_points() const;
  size_t bezier3_points_count() const;

  enum class SceneObjectError : uint8_t {
    NotAPoint     = 0,
    InvalidHandle = 1,
    NotFound      = 2,
  };

  std::expected<void, SceneObjectError> add(const SceneObjectHandle& handle);
  std::expected<void, SceneObjectError> remove(const SceneObjectHandle& handle);
  std::expected<void, SceneObjectError> move_before(const SceneObjectHandle& dest, const SceneObjectHandle& obj);
  std::expected<void, SceneObjectError> move_after(const SceneObjectHandle& dest, const SceneObjectHandle& obj);

  void update();
  void on_delete();

 private:
  void update_indices_from(size_t start_idx);

 private:
  friend Scene;
  friend SceneObject;
};

// ---------------------------------------------------------------------------------------------------------------------
// - PatchSurfaceType --------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
concept CPatchSurfaceType = requires {
  { T::type_name() } -> std::same_as<zstring_view>;
};

class BezierPatches {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Bezier Patches"; }
};

class BPatches {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "B-Patches"; }
};

using PatchSurfaceVariant = std::variant<BezierPatches, BPatches>;

// Check if all of the variant types are valid
template <typename Variant>
struct ValidateSurfacePatchesVariantTypes;
template <typename... Types>
struct ValidateSurfacePatchesVariantTypes<std::variant<Types...>> {
  static constexpr bool kAreVariantTypesValid = (CSceneObjectType<Types> && ...);
};
static_assert(ValidateSceneObjectVariantTypes<SceneObjectVariant>::kAreVariantTypesValid,
              "Not all variant types satisfy CPatchSurfaceType");

// ---------------------------------------------------------------------------------------------------------------------
// - PatchSurface ------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class PatchSurface : public ObjectBase<PatchSurface, PatchSurfaceVariant>, public PointListObjectBase<PatchSurface> {
 public:
  PatchSurface() = delete;
  PatchSurface(const PatchSurfaceHandle& handle, Scene& scene)
      : ObjectBase<PatchSurface, PatchSurfaceVariant>(handle, scene) {}
  ERAY_DEFAULT_MOVE(PatchSurface)
  ERAY_DELETE_COPY(PatchSurface)

  std::generator<eray::math::Vec3f> polyline_points() const { co_return; }
  size_t polyline_points_count() const { return 0; }

  std::generator<eray::math::Vec3f> bezier3_points() const { co_return; }
  size_t bezier3_points_count() const { return 0; }

  void update() {}
  void on_delete() {}
};

}  // namespace mini
