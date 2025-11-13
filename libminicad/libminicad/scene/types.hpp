#pragma once

#include <concepts>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/transform3_fwd.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <utility>

namespace mini {
class ParamSpaceTrimmingData;
class ParamSpaceTrimmingDataManager;

template <typename T>
concept CParametricCurveObject = requires(T t, float param) {
  typename T::Variant;
  { t.frenet_frame(param) } -> std::same_as<eray::math::Mat4f>;
  { t.evaluate(param) } -> std::same_as<eray::math::Vec3f>;
  { t.aabb_bounding_box() } -> std::same_as<std::pair<eray::math::Vec3f, eray::math::Vec3f>>;
};

struct SecondDerivatives {
  eray::math::Vec3f dp_duu;
  eray::math::Vec3f dp_dvv;
  eray::math::Vec3f dp_duv;  // According to Schwarz's Theorem, dp_duv = dp_dvu for C^2 functions
};

template <typename T>
concept CParametricSurfaceObject = requires(T t, float param1, float param2, ParamSpaceTrimmingData&& data) {
  typename T::Variant;
  { t.frenet_frame(param1, param1) } -> std::same_as<eray::math::Mat4f>;
  { t.evaluate(param1, param2) } -> std::same_as<eray::math::Vec3f>;
  { t.evaluate_derivatives(param1, param2) } -> std::same_as<std::pair<eray::math::Vec3f, eray::math::Vec3f>>;
  { t.evaluate_second_derivatives(param1, param2) } -> std::same_as<SecondDerivatives>;
  { t.aabb_bounding_box() } -> std::same_as<std::pair<eray::math::Vec3f, eray::math::Vec3f>>;
  { t.trimming_manager() } -> std::same_as<ParamSpaceTrimmingDataManager&>;
  { t.update_trimming_txt() } -> std::same_as<void>;
  { t.txt_handle() } -> std::same_as<const TextureHandle&>;
};

template <typename T>
concept CTransformableObject = requires(T t) {
  typename T::Variant;
  { t.transform() } -> std::same_as<eray::math::Transform3f&>;
  { std::as_const(t).transform() } -> std::same_as<const eray::math::Transform3f&>;
};

}  // namespace mini
