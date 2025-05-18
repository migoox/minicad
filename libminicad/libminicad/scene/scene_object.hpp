#pragma once

#include <concepts>
#include <cstdint>
#include <generator>
#include <liberay/math/transform3.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/variant_match.hpp>
#include <liberay/util/zstring_view.hpp>
#include <libminicad/scene/point_list.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <variant>

#define MINI_VALIDATE_VARIANT_TYPES(TVariant, CVariant)                      \
  template <typename Variant>                                                \
  struct ValidateVariant_##TVariant;                                         \
                                                                             \
  template <typename... Types>                                               \
  struct ValidateVariant_##TVariant<std::variant<Types...>> {                \
    static constexpr bool kAreVariantTypesValid = (CVariant<Types> && ...);  \
  };                                                                         \
                                                                             \
  static_assert(ValidateVariant_##TVariant<TVariant>::kAreVariantTypesValid, \
                "Not all variant types satisfy the required constraint")

namespace mini {

// ---------------------------------------------------------------------------------------------------------------------
// - ObjectBase --------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
concept CObject =
    requires(T t, const eray::util::Handle<T>& handle, Scene& scene, std::string&& name, std::size_t idx) {
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
    };

template <typename T>
concept CObjectVariant = requires {
  { T::type_name() } -> std::same_as<zstring_view>;
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
  auto point_objects() { return points_.point_objects(); }
  auto point_objects() const { return points_.point_objects(); }
  auto point_handles() { return points_.point_handles(); }
  auto point_handles() const { return points_.point_handles(); }

  auto points() const {
    return point_objects() | std::views::transform([](const auto& s) { return s.transform.pos(); });
  }

  bool contains(const SceneObjectHandle& handle) { return points_.contains(handle); }

  OptionalObserverPtr<SceneObject> point(const SceneObjectHandle& handle) { return points_.point(handle); }

  std::optional<size_t> point_idx(const SceneObjectHandle& handle) { return points_.point_idx(handle); }

 protected:
  PointList points_;

 private:
  friend Curve;
  friend PatchSurface;

  PointListObjectBase() = default;
};

// ---------------------------------------------------------------------------------------------------------------------
// - SceneObjectType
// ---------------------------------------------------------------------------------------------------
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
  ERAY_DEFAULT_MOVE(SceneObject)
  ERAY_DELETE_COPY(SceneObject)
  SceneObject(SceneObjectHandle handle, Scene& scene);

  void update();
  void on_delete();
  bool can_be_deleted();

 public:
  eray::math::Transform3f transform;

 private:
  friend Scene;
  friend Curve;
  friend PatchSurface;

 private:
  std::unordered_set<CurveHandle> curves_;
  std::unordered_set<PatchSurfaceHandle> patch_surfaces_;
};

// ---------------------------------------------------------------------------------------------------------------------
// - CurveType -----------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

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

using CurveVariant = std::variant<Polyline, MultisegmentBezierCurve, BSplineCurve, NaturalSplineCurve>;
MINI_VALIDATE_VARIANT_TYPES(CurveVariant, CCurveType);

// ---------------------------------------------------------------------------------------------------------------------
// - Curve -------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class Curve : public ObjectBase<Curve, CurveVariant>, public PointListObjectBase<Curve> {
 public:
  Curve() = delete;
  Curve(const CurveHandle& handle, Scene& scene);
  ERAY_DEFAULT_MOVE(Curve)
  ERAY_DELETE_COPY(Curve)

  std::generator<eray::math::Vec3f> polyline_points() const;
  size_t polyline_points_count() const;

  std::generator<eray::math::Vec3f> bezier3_points() const;
  size_t bezier3_points_count() const;

  enum class SceneObjectError : uint8_t {
    NotAPoint     = static_cast<uint8_t>(PointList::OperationError::NotAPoint),
    NotFound      = static_cast<uint8_t>(PointList::OperationError::NotFound),
    InvalidHandle = 2,
  };

  std::expected<void, SceneObjectError> add(const SceneObjectHandle& handle);
  std::expected<void, SceneObjectError> remove(const SceneObjectHandle& handle);
  std::expected<void, SceneObjectError> move_before(const SceneObjectHandle& dest, const SceneObjectHandle& obj);
  std::expected<void, SceneObjectError> move_after(const SceneObjectHandle& dest, const SceneObjectHandle& obj);

  void update();
  void on_delete();
  bool can_be_deleted() { return true; }

 private:
  void update_indices_from(size_t start_idx);

 private:
  friend Scene;
  friend SceneObject;
};

// ---------------------------------------------------------------------------------------------------------------------
// - PatchSurfaceType --------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class BezierPatches {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Bezier Patches"; }
};

class BPatches {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "B-Patches"; }
};

template <typename T>
concept CPatchSurfaceType = requires {
  { T::type_name() } -> std::same_as<zstring_view>;
};

using PatchSurfaceVariant = std::variant<BezierPatches, BPatches>;
MINI_VALIDATE_VARIANT_TYPES(PatchSurfaceVariant, CPatchSurfaceType);

// ---------------------------------------------------------------------------------------------------------------------
// - PatchSurface ------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class PatchSurface : public ObjectBase<PatchSurface, PatchSurfaceVariant>, public PointListObjectBase<PatchSurface> {
 public:
  PatchSurface() = delete;
  PatchSurface(const PatchSurfaceHandle& handle, Scene& scene);

  ERAY_DEFAULT_MOVE(PatchSurface)
  ERAY_DELETE_COPY(PatchSurface)

  static constexpr int kPatchSize = 4;

  std::generator<eray::math::Vec3f> polyline_points() const { co_return; }
  size_t polyline_points_count() const { return 0; }

  std::generator<eray::math::Vec3f> bezier3_points() const { co_return; }
  size_t bezier3_points_count() const { return 0; }

  void make_plane(eray::math::Vec2u dim, eray::math::Vec2f size);

  void update() {}
  void on_delete() {}
  bool can_be_deleted() { return true; }

  const eray::math::Vec2u& dimensions() const { return dim_; }

 private:
  static size_t find_idx(size_t patch_x, size_t patch_y, size_t point_x, size_t point_y, size_t dim_x) {
    return ((kPatchSize - 1) * dim_x + 1) * ((kPatchSize - 1) * patch_y + point_y) +
           ((kPatchSize - 1) * patch_x + point_x);
  }

  eray::math::Vec2u dim_;
};

}  // namespace mini
