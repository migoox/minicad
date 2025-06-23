#pragma once

#include <concepts>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/transform3_fwd.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <utility>

#include "libminicad/scene/intersection_curve.hpp"

namespace mini {

template <typename T>
concept CParametricCurveObject = requires(T t, float param) {
  typename T::Variant;
  { t.frenet_frame(param) } -> std::same_as<eray::math::Mat4f>;
  { t.evaluate(param) } -> std::same_as<eray::math::Vec3f>;
  { t.aabb_bounding_box() } -> std::same_as<std::pair<eray::math::Vec3f, eray::math::Vec3f>>;
};

template <typename T>
concept CParametricSurfaceObject = requires(T t, float param1, float param2, const IntersectionCurve& curve) {
  typename T::Variant;
  { t.frenet_frame(param1, param1) } -> std::same_as<eray::math::Mat4f>;
  { t.evaluate(param1, param2) } -> std::same_as<eray::math::Vec3f>;
  { t.evaluate_derivatives(param1, param2) } -> std::same_as<std::pair<eray::math::Vec3f, eray::math::Vec3f>>;
  { t.aabb_bounding_box() } -> std::same_as<std::pair<eray::math::Vec3f, eray::math::Vec3f>>;
  { t.add_intersection_curve(curve) } -> std::same_as<void>;
  { t.remove_intersection_curve(curve) } -> std::same_as<void>;
};

template <typename T>
concept CTransformableObject = requires(T t) {
  typename T::Variant;
  { t.transform() } -> std::same_as<eray::math::Transform3f&>;
  { std::as_const(t).transform() } -> std::same_as<const eray::math::Transform3f&>;
};

}  // namespace mini
