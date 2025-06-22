#pragma once

#include <liberay/math/vec_fwd.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <optional>

#include "libminicad/scene/patch_surface.hpp"

namespace mini {

class IntersectionFinder {
 public:
  struct Result {};

  /**
   * @brief Finds intersection between two parametric surfaces. Returns nullopt if no intersections are found.
   *
   * @param h1
   * @param h2
   * @return std::optional<Result>
   */
  static std::optional<Result> find_intersections(Scene& scene, const PatchSurfaceHandle& h1,
                                                  const PatchSurfaceHandle& h2);

 private:
  static bool aabb_intersects(const std::pair<eray::math::Vec3f, eray::math::Vec3f>& a,
                              const std::pair<eray::math::Vec3f, eray::math::Vec3f>& b);

  static eray::math::Vec4f gradient_descent(const eray::math::Vec4f& init, float learning_rate, float tolerance,
                                            int max_iters, const std::function<float(const eray::math::Vec4f&)>& func,
                                            const std::function<eray::math::Vec4f(const eray::math::Vec4f&)>& grad);
  static eray::math::Vec4f newton_start_point_refiner(const eray::math::Vec4f& init, PatchSurface& ps1,
                                                      PatchSurface& ps2, int iters,
                                                      const std::function<float(const eray::math::Vec4f&)>& err_func);

  static eray::math::Vec4f newton_next_point(const eray::math::Vec4f& start, PatchSurface& ps1, PatchSurface& ps2,
                                             int iters, bool reverse = false);
};

}  // namespace mini
