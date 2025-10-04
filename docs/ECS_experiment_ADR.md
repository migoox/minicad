```cpp

#pragma once

#include <cstdint>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/transform3_fwd.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/util/object_handle.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

namespace math = eray::math;

namespace mini {

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
  std::optional<math::Vec3f> point(SceneObjectHandle handle);
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

}  // namespace mini


```
