#pragma once

#include <liberay/math/vec_fwd.hpp>
#include <liberay/util/zstring_view.hpp>
#include <libminicad/scene/scene_object.hpp>

#include "libminicad/scene/types.hpp"

namespace mini {
// ---------------------------------------------------------------------------------------------------------------------
// - CurveType ---------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct Polyline {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Polyline"; }
  void on_point_update(Curve&, const PointObject&, const Point&) {}
  void on_point_add(Curve&, const PointObject&, const Point&) {}
  void on_point_remove(Curve&, const PointObject&, const Point&) {}
  void on_curve_reorder(Curve&) {}
  std::generator<eray::math::Vec3f> bezier3_points(ref<const Curve> base) const;
  size_t bezier3_points_count(ref<const Curve> base) const;
};

struct MultisegmentBezierCurve {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Multisegment C0 Bezier Curve"; }
  void on_point_update(Curve&, const PointObject&, const Point&) {}
  void on_point_add(Curve&, const PointObject&, const Point&) {}
  void on_point_remove(Curve&, const PointObject&, const Point&) {}
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
  void update_bernstein_points(const Curve& base, const PointObjectHandle& handle);

  const std::vector<eray::math::Vec3f>& bernstein_points() const { return bezier_points_; }

  std::optional<eray::math::Vec3f> bernstein_point(size_t idx) const {
    if (bezier_points_.size() > idx) {
      return bezier_points_[idx];
    }

    return std::nullopt;
  }

  void set_bernstein_point(Curve& base, size_t idx, const eray::math::Vec3f& point);

  bool contains(size_t idx) { return bezier_points_.size() > idx; }

  void on_point_update(Curve& base, const PointObject&, const Point&);
  void on_point_add(Curve& base, const PointObject&, const Point&);
  void on_point_remove(Curve& base, const PointObject&, const Point&);
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
   * @brief Describes one segment in the natural spline using a bezier basis.
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

  void on_point_update(Curve& base, const PointObject&, const Point&) { update(base); }
  void on_point_add(Curve& base, const PointObject&, const Point&) { update(base); }
  void on_point_remove(Curve& base, const PointObject&, const Point&) { update(base); }
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
concept CCurveType = requires(T t, Curve& base, ref<const Curve> base_ref, const PointObject& point_obj,
                              const Point& point, float tparam) {
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

  const std::vector<eray::math::Vec3f>& bezier3_points();

  enum class SceneObjectError : uint8_t {
    NotAPoint     = static_cast<uint8_t>(PointList::OperationError::NotAPoint),
    NotFound      = static_cast<uint8_t>(PointList::OperationError::NotFound),
    InvalidHandle = 2,
  };

  std::expected<void, SceneObjectError> push_back(const PointObjectHandle& handle);
  std::expected<void, SceneObjectError> remove(const PointObjectHandle& handle);
  std::expected<void, SceneObjectError> move_before(size_t dest_idx, size_t source_idx);
  std::expected<void, SceneObjectError> move_after(size_t dest_idx, size_t source_idx);

  void update();
  void on_delete();
  bool can_be_deleted() const { return true; }

  void clone_to(Curve& obj) const;

  /**
   * @brief Returns a matrix representing a frame (Frenet basis).
   *
   * @param t
   * @return eray::math::Mat4f
   */
  [[nodiscard]] eray::math::Mat4f frenet_frame(float t);

  [[nodiscard]] eray::math::Vec3f evaluate(float t);

  [[nodiscard]] std::pair<eray::math::Vec3f, eray::math::Vec3f> aabb_bounding_box();

 private:
  void update_indices_from(size_t start_idx);
  void mark_bezier3_dirty() { bezier_dirty_ = true; }
  void refresh_bezier3_if_dirty();

 private:
  friend Scene;
  friend PointObject;

  std::vector<eray::math::Vec3f> bezier3_points_;
  bool bezier_dirty_;
};

static_assert(CParametricCurveObject<Curve>);

}  // namespace mini
