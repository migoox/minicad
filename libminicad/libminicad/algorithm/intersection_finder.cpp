#include <liberay/math/vec.hpp>
#include <liberay/util/logger.hpp>
#include <libminicad/algorithm/intersection_finder.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene.hpp>
#include <optional>

namespace mini {

namespace math = eray::math;

bool IntersectionFinder::aabb_intersects(const std::pair<eray::math::Vec3f, eray::math::Vec3f>& a,
                                         const std::pair<eray::math::Vec3f, eray::math::Vec3f>& b) {
  const auto& min_a = a.first;
  const auto& max_a = a.second;
  const auto& min_b = b.first;
  const auto& max_b = b.second;

  // Check for separation in x, y, z axes
  if (max_a.x < min_b.x || min_a.x > max_b.x) {
    return false;
  }
  if (max_a.y < min_b.y || min_a.y > max_b.y) {
    return false;
  }
  if (max_a.z < min_b.z || min_a.z > max_b.z) {
    return false;
  }

  return true;
}

eray::math::Vec4f IntersectionFinder::gradient_descent(
    const eray::math::Vec4f& init, const float learning_rate, const float tolerance, const int max_iters,
    const std::function<float(const eray::math::Vec4f&)>& func,
    const std::function<eray::math::Vec4f(const eray::math::Vec4f&)>& grad) {
  auto result = init;

  auto prev_val = func(result);
  for (auto i = 0; i < max_iters; ++i) {
    auto old_result = result;
    result          = result - learning_rate * grad(result);
    result.x        = result.x;
    result.y        = result.y;
    result.z        = old_result.z;
    result.w        = old_result.w;

    old_result = result;
    result     = result - learning_rate * grad(result);
    result.x   = old_result.x;
    result.y   = old_result.y;
    result.z   = result.z;
    result.w   = result.w;

    auto val = func(result);

    if (std::abs(prev_val - val) < tolerance) {
      break;
    }
    prev_val = val;
  }

  return result;
}

std::optional<IntersectionFinder::Result> IntersectionFinder::find_intersections(Scene& scene,
                                                                                 const PatchSurfaceHandle& h1,
                                                                                 const PatchSurfaceHandle& h2) {
  auto opt1 = scene.arena<PatchSurface>().get_obj(h1);
  if (!opt1) {
    return std::nullopt;
  }
  auto opt2 = scene.arena<PatchSurface>().get_obj(h2);
  if (!opt2) {
    return std::nullopt;
  }

  auto& ps1 = **opt1;
  auto& ps2 = **opt2;

  auto bb1 = ps1.aabb_bounding_box();
  auto bb2 = ps2.aabb_bounding_box();
  if (!aabb_intersects(bb1, bb2)) {
    return std::nullopt;
  }

  auto len_func = [&](const math::Vec4f& p) {
    auto diff = ps1.evaluate(p.x, p.y) - ps2.evaluate(p.z, p.w);
    // scene.renderer().debug_point(ps1.evaluate(p.x, p.y));
    // scene.renderer().debug_point(ps1.evaluate(p.z, p.w));
    auto res = math::dot(diff, diff);
    return res;
  };

  auto len_func_grad = [&](const math::Vec4f& p) {
    auto diff = ps1.evaluate(p.x, p.y) - ps2.evaluate(p.z, p.w);

    const auto& [ps1_dx, ps1_dy] = ps1.evaluate_derivatives(p.x, p.y);
    const auto& [ps2_dz, ps2_dw] = ps2.evaluate_derivatives(p.z, p.w);

    return math::Vec4f{
        2.F * math::dot(ps1_dx, diff),
        2.F * math::dot(ps1_dy, diff),
        -2.F * math::dot(ps2_dz, diff),
        -2.F * math::dot(ps2_dw, diff),
    };
  };

  auto result = gradient_descent(math::Vec4f::filled(0.5F), 0.01F, 0.001F, 200, len_func, len_func_grad);
  eray::util::Logger::info("From gradient descent: {}", result);

  scene.renderer().debug_point(ps1.evaluate(result.x, result.y));
  scene.renderer().debug_point(ps2.evaluate(result.z, result.w));
  scene.renderer().debug_line(ps1.evaluate(result.x, result.y), ps2.evaluate(result.z, result.w));

  return std::nullopt;
}

}  // namespace mini
