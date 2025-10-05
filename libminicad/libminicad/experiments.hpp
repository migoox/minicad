#pragma once

#include <memory>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include "liberay/math/mat_fwd.hpp"
#include "liberay/math/transform3_fwd.hpp"
#include "liberay/math/vec.hpp"
#include "liberay/math/vec_fwd.hpp"
#include "liberay/util/object_handle.hpp"

namespace mini {
namespace math = eray::math;

struct SceneObjectTag;
using SceneObjectHandle = eray::util::Handle<SceneObjectTag>;

enum class ParamSurfaceKind : uint8_t {
  None,
  Torus,
  Custom,
};

enum class SceneObjectKind : uint8_t {
  Point,          // Vec3f, References
  ParamSurface,   // Params, TesselationInfo
  Curve,          // ControlPoints, Points
  PatchSurface,   // Dimensions, FillInSurfaceRefs, ControlPoints, Points, TesselationInfo, TrimmingInfo
  FillInSurface,  // ControlPoints, Points, TesselationInfo
  ApproxCurve,    // Points
};

enum class CurveKind : uint8_t {
  None,
  Bezier,
  BSpline,
  NaturalSpline,
};

enum class PatchSurfaceKind : uint8_t {
  None,
  Bezier,
  BPatch,
};

struct SceneObjectInfo {
  SceneObjectKind object_kind;
  ParamSurfaceKind param_surface_kind;
  CurveKind curve_kind;
  PatchSurfaceKind patch_surface_kind;
};

using BoundingBox = std::pair<math::Vec3f, math::Vec3f>;
using Derivatives = std::pair<math::Vec3f, math::Vec3f>;

struct Error {};

// components ////////////////
using Point           = math::Vec3f;
using References      = std::vector<SceneObjectHandle>;
using Points          = std::vector<math::Vec3f>;
using TesselationInfo = math::Vec2i;
using Dimensions      = math::Vec2u;
struct Params {
  ParamSurfaceKind params_kind;
  float param1;
  float param2;
  float param3;
};
using Transform = eray::math::Transform3f;
//////////////////////////////

class SceneApi {
 public:
  template <typename T>
  using Map = std::unordered_map<SceneObjectHandle, T>;

  std::optional<SceneObjectInfo> info(SceneObjectHandle handle);
  std::optional<eray::math::Vec3f> point(SceneObjectHandle handle);
  std::optional<std::vector<math::Vec3f>> bezier_points(SceneObjectHandle handle);

  SceneObjectHandle clone(SceneObjectHandle handle);

  BoundingBox aabb_bounding_box(SceneObjectHandle handle);

  math::Mat4f frenet_frame_1d(SceneObjectHandle handle, float t);
  math::Mat4f evaluate(SceneObjectHandle handle, float t);

  math::Mat4f frenet_frame_2d(SceneObjectHandle handle, float u, float v);
  math::Vec3f evaluate_2d(SceneObjectHandle handle, float u, float v);
  Derivatives evaluate_derivatives2d(SceneObjectHandle handle, float t);

 private:
  Map<Point> point_value_;
  Map<References> references_;
  Map<Points> points_;
  Map<TesselationInfo> tesselation_;
  Map<Dimensions> dimensions_;
  Map<Params> params_;
  Map<Transform> transform_;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using ControlPointId = uint32_t;

struct ControlPointList;
struct ControlPointMesh;

struct ControlPointsManager {
  std::vector<math::Vec3f> points_;
  std::vector<uint32_t> reference_count_;
  std::vector<std::unordered_set<ControlPointList*>> referenced_by_lists_;
  std::vector<std::unordered_set<ControlPointMesh*>> referenced_by_meshes_;

  void collapse_points(ControlPointId point1, ControlPointId point2);
  void collapse_points(const std::vector<ControlPointId>& points);

  ControlPointId create_control_point(math::Vec3f value);
  void update_control_point(ControlPointId id, math::Vec3f value);
  void is_control_point_referenced(ControlPointId id);
  bool try_delete_control_point(ControlPointId id);
  auto control_points_view(const std::vector<ControlPointId>& point_refs) {
    return point_refs | std::ranges::views::transform([this](auto&& point_ref) { return points_[point_ref]; });
  }
};

struct ControlPointList {
  static std::shared_ptr<ControlPointList> create(const std::shared_ptr<ControlPointsManager>& manager);

  std::shared_ptr<ControlPointsManager> manager_;
  std::vector<ControlPointId> control_point_refs_;
  size_t list_index_;

  void push_back(ControlPointId id);
  void push_back(math::Vec3f point);
  void remove(ControlPointId id);
  void move_before(size_t dest_idx, size_t source_idx);
  void move_after(size_t dest_idx, size_t source_idx);

  auto control_points_view() { return manager_->control_points_view(control_point_refs_); }

  std::shared_ptr<ControlPointList> clone();
};

struct ControlPointMesh {
  static std::shared_ptr<ControlPointMesh> create(const std::shared_ptr<ControlPointsManager>& manager);

  std::shared_ptr<ControlPointsManager> manager_;
  std::vector<ControlPointId> control_point_refs_;
  size_t mesh_index_;

  math::Vec2i dimensions_;
  uint32_t step_;
  bool last_column_wrapped_;

  void push_patch_rows_top(uint32_t count = 1);
  void remove_patch_rows_top(uint32_t count = 1);

  void push_patch_rows_bottom(uint32_t count = 1);
  void remove_patch_rows_bottom(uint32_t count = 1);

  void push_patch_columns_left(uint32_t count = 1);
  void remove_patch_columns_left(uint32_t count = 1);

  void push_patch_columns_right(uint32_t count = 1);
  void remove_patch_columns_right(uint32_t count = 1);

  std::shared_ptr<ControlPointMesh> clone();
};

class IParameterisation1D {
 public:
  virtual eray::math::Mat4f frenet_frame(float t)        = 0;
  virtual eray::math::Vec3f evaluate(float t)            = 0;
  virtual eray::math::Vec3f evaluate_derivative(float t) = 0;

  virtual ~IParameterisation1D() = default;
};

class IParameterisation2D {
 public:
  virtual eray::math::Mat4f frenet_frame(float u, float v)                                       = 0;
  virtual eray::math::Vec3f evaluate(float u, float v)                                           = 0;
  virtual std::pair<eray::math::Vec3f, eray::math::Vec3f> evaluate_derivatives(float u, float v) = 0;

  virtual ~IParameterisation2D() = default;
};

struct CurveBezier3Basis : public IParameterisation1D {
  static constexpr uint32_t kSegmentLength = 4;
  static constexpr uint32_t kStep          = 3;

  template <std::ranges::input_range Range>
  CurveBezier3Basis create_polyline(Range&& control_points);

  template <std::ranges::input_range Range>
  CurveBezier3Basis create_bspline(Range&& control_points);

  template <std::ranges::input_range Range>
  CurveBezier3Basis create_natural_spline(Range&& control_points);

  template <std::ranges::input_range Range>
  CurveBezier3Basis create_multisegment_bezier3(Range&& control_points);

  std::vector<math::Vec3f> bezier3_points_;
};

}  // namespace mini
