#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <liberay/math/transform3.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/zstring_view.hpp>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

namespace mini {

using SceneObjectId     = std::uint32_t;
using PointListObjectId = std::uint32_t;

class Scene;

class SceneObject;
using SceneObjectHandle = eray::util::Handle<SceneObject>;

class PointListObject;
using PointListObjectHandle = eray::util::Handle<PointListObject>;

template <typename T>
concept CObjectHandle = std::is_same_v<T, SceneObjectHandle> || std::is_same_v<T, PointListObjectHandle>;

using ObjectHandle = std::variant<SceneObjectHandle, PointListObjectHandle>;

class PointListObject;

template <typename T>
using ObserverPtr = eray::util::ObserverPtr<T>;

template <typename T>
using OptionalObserverPtr = std::optional<eray::util::ObserverPtr<T>>;

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

class SceneObject {
 public:
  SceneObject() = delete;
  explicit SceneObject(SceneObjectHandle handle, Scene& scene) : handle_(handle), scene_(scene) {}
  ~SceneObject();

  SceneObjectId id() const { return handle_.obj_id; }
  const SceneObjectHandle& handle() const { return handle_; }

  void update();
  size_t order_ind() const { return order_ind_; }
  Scene& scene() { return scene_; }
  const Scene& scene() const { return scene_; }

  template <typename Type>
    requires std::is_constructible_v<SceneObjectVariant, Type>
  bool has_type() {
    return std::holds_alternative<Type>(this->object);
  }

  /**
   * @brief If the SceneObject is not of the requested Type, this function will fail. Use with caution.
   *
   * @tparam Type
   * @return requires&
   */
  template <typename Type>
    requires std::is_constructible_v<SceneObjectVariant, Type>
  Type& get_variant() {
    assert(std::holds_alternative<Type>(this->object) && "PointListObject is not of the requested type.");
    return std::get<Type>(this->object);
  }

  template <typename Visitor>
  void visit(Visitor&& visitor) {
    std::visit(std::forward(visitor), this->object);
  }

 public:
  eray::math::Transform3f transform;
  std::string name;
  SceneObjectVariant object;

 private:
  friend Scene;
  friend PointListObject;

  size_t order_ind_{0};

  std::unordered_set<PointListObjectHandle> point_lists_;

  SceneObjectHandle handle_;
  Scene& scene_;  // NOLINT (lifetime of the scene always exceeds the lifetime of the scene object)
};

// ---------------------------------------------------------------------------------------------------------------------
// - PointListObjectType -----------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
concept CPointListObjectType = requires(T t, PointListObject& base, const SceneObject& point_obj, const Point& point) {
  { T::type_name() } -> std::same_as<zstring_view>;
  { t.on_point_update(base, point_obj, point) } -> std::same_as<void>;
  { t.on_point_add(base, point_obj, point) } -> std::same_as<void>;
  { t.on_point_remove(base, point_obj, point) } -> std::same_as<void>;
  { t.on_point_list_reorder(base) } -> std::same_as<void>;
};

struct Polyline {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Polyline"; }
  void on_point_update(PointListObject&, const SceneObject&, const Point&) {}
  void on_point_add(PointListObject&, const SceneObject&, const Point&) {}
  void on_point_remove(PointListObject&, const SceneObject&, const Point&) {}
  void on_point_list_reorder(PointListObject&) {}
};

struct MultisegmentBezierCurve {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Multisegment C0 Bezier Curve"; }
  void on_point_update(PointListObject&, const SceneObject&, const Point&) {}
  void on_point_add(PointListObject&, const SceneObject&, const Point&) {}
  void on_point_remove(PointListObject&, const SceneObject&, const Point&) {}
  void on_point_list_reorder(PointListObject&) {}
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
  void reset_bernstein_points(const PointListObject& base);

  /**
   * @brief Updates the de Boor points (scene point objects) basing on the de Boor points.
   *
   */
  void update_de_boor_points(PointListObject& base);

  /**
   * @brief Updates the Bernstein points (scene point objects) basing on the de Boor points.
   *
   */
  void update_bernstein_points(PointListObject& base, const SceneObjectHandle& handle);

  const std::vector<eray::math::Vec3f>& bernstein_points() const { return bernstein_points_; }

  std::optional<eray::math::Vec3f> bernstein_point(size_t idx) const {
    if (bernstein_points_.size() > idx) {
      return bernstein_points_[idx];
    }

    return std::nullopt;
  }

  void set_bernstein_point(PointListObject& base, size_t idx, const eray::math::Vec3f& point);

  bool contains(size_t idx) { return bernstein_points_.size() > idx; }

  void on_point_update(PointListObject& base, const SceneObject&, const Point&);
  void on_point_add(PointListObject& base, const SceneObject&, const Point&);
  void on_point_remove(PointListObject& base, const SceneObject&, const Point&);
  void on_point_list_reorder(PointListObject& base);

 private:
  /**
   * @brief Updates the bernstein points basing on the de Boor points (scene point objects). Invoked automatically
   * when the de Boor points are marked dirty.
   *
   * @param base
   * @param idx: represents first control point (de boor point) index in the segment of length 4
   */
  void update_bernstein_segment(const PointListObject& base, int cp_idx);

 private:
  std::vector<eray::math::Vec3f> bernstein_points_;
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
  void reset_segments(PointListObject& base);

  const std::vector<Segment>& segments() const { return segments_; }
  const std::vector<eray::math::Vec3f>& unique_points() const { return unique_points_; }

  void on_point_update(PointListObject& base, const SceneObject&, const Point&) { update(base); }
  void on_point_add(PointListObject& base, const SceneObject&, const Point&) { update(base); }
  void on_point_remove(PointListObject& base, const SceneObject&, const Point&) { update(base); }
  void on_point_list_reorder(PointListObject& base) { update(base); }

 private:
  void update(PointListObject& base);

 private:
  std::vector<Segment> segments_;
  std::vector<eray::math::Vec3f> unique_points_;
};

using PointListObjectVariant = std::variant<Polyline, MultisegmentBezierCurve, BSplineCurve, NaturalSplineCurve>;

// Check if all of the variant types are valid
template <typename Variant>
struct ValidatePointListObjectVariantTypes;
template <typename... Types>
struct ValidatePointListObjectVariantTypes<std::variant<Types...>> {
  static constexpr bool kAreVariantTypesValid = (CPointListObjectType<Types> && ...);
};
static_assert(ValidatePointListObjectVariantTypes<PointListObjectVariant>::kAreVariantTypesValid,
              "Not all variant types satisfy CPointListObjectType");

// ---------------------------------------------------------------------------------------------------------------------
// - PointListObject ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class PointListObject {
 public:
  PointListObject() = delete;
  explicit PointListObject(PointListObjectHandle handle, Scene& scene) : handle_(handle), scene_(scene) {}
  ~PointListObject();

  auto point_objects() {
    return points_ | std::ranges::views::transform([](auto& ref) -> auto& { return ref.get(); });
  }

  auto point_objects() const {
    return points_ | std::ranges::views::transform([](const auto& ref) -> const auto& { return ref.get(); });
  }

  auto point_handles() { return points_map_ | std::views::keys; }

  auto points() {
    return point_objects() | std::views::transform([](auto& s) { return s.transform.pos(); });
  }

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

  enum class SceneObjectError : uint8_t {
    NotAPoint        = 0,
    HandleIsNotValid = 1,
    NotFound         = 2,
  };

  std::expected<void, SceneObjectError> add(const SceneObjectHandle& handle);
  std::expected<void, SceneObjectError> remove(const SceneObjectHandle& handle);
  std::expected<void, SceneObjectError> move_before(const SceneObjectHandle& dest, const SceneObjectHandle& obj);
  std::expected<void, SceneObjectError> move_after(const SceneObjectHandle& dest, const SceneObjectHandle& obj);

  PointListObjectId id() const { return handle_.obj_id; }
  const PointListObjectHandle& handle() const { return handle_; }

  void update();
  size_t order_ind() const { return order_ind_; }

  Scene& scene() { return scene_; }
  const Scene& scene() const { return scene_; }

  template <typename Type>
    requires std::is_constructible_v<PointListObjectVariant, Type>
  bool has_type() {
    return std::holds_alternative<Type>(this->object);
  }

  /**
   * @brief If the PointListObject is not of the requested Type, this function will fail. Use with caution.
   *
   * @tparam Type
   * @return requires&
   */
  template <typename Type>
    requires std::is_constructible_v<PointListObjectVariant, Type>
  Type& get_variant() {
    assert(std::holds_alternative<Type>(this->object) && "PointListObject is not of the requested type.");
    return std::get<Type>(this->object);
  }

  template <typename Visitor>
  void visit(Visitor&& visitor) {
    std::visit(std::forward(visitor), this->object);
  }

 private:
  void update_indices_from(size_t start_idx);

 public:
  std::string name;
  PointListObjectVariant object;

 private:
  friend Scene;
  friend SceneObject;

  PointListObjectHandle handle_;
  Scene& scene_;  // NOLINT (lifetime of the scene always exceeds the lifetime of the point list object)

  size_t order_ind_{0};

  std::vector<std::reference_wrapper<SceneObject>> points_;
  std::unordered_map<SceneObjectHandle, size_t> points_map_;
};

}  // namespace mini
