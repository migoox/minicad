#pragma once

#include <liberay/math/vec.hpp>

namespace mini {

using Vec3f = eray::math::Vec3f;

inline Vec3f bezier3(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2, const Vec3f& p3, float t) {
  float u  = 1.0F - t;
  float b3 = t * t * t;
  float b2 = 3.0F * t * t * u;
  float b1 = 3.0F * t * u * u;
  float b0 = u * u * u;
  return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

inline Vec3f bezier3_dt(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2, const Vec3f& p3, float t) {
  float u = 1.0F - t;
  return 3.0F * u * u * (p1 - p0) + 6.0F * u * t * (p2 - p1) + 3.0F * t * t * (p3 - p2);
}

inline Vec3f bezier3_dtt(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2, const Vec3f& p3, float t) {
  float u = 1.0F - t;
  return 6.0F * u * (p2 - 2.0F * p1 + p0) + 6.0F * t * (p3 - 2.0F * p2 + p1);
}

}  // namespace mini
