#include <algorithm>
#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/util/logger.hpp>
#include <libminicad/algorithm/intersection_finder.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene.hpp>
#include <optional>
#include <random>
#include <ranges>
#include <vector>

namespace mini {

namespace math = eray::math;

void IntersectionFinder::Curve::push_point(const eray::math::Vec4f& params, PatchSurface& surface) {
  params_surface1.emplace_back(params.x, params.y);
  params_surface2.emplace_back(params.z, params.w);
  points.push_back(surface.evaluate(params.x, params.y));
}

void IntersectionFinder::Curve::reverse() {
  std::ranges::reverse(points);
  std::ranges::reverse(params_surface1);
  std::ranges::reverse(params_surface2);
}

void IntersectionFinder::Curve::fill_textures() {
  fill_texture(txt_params_space1, params_surface1);
  fill_texture(txt_params_space2, params_surface2);
}

void IntersectionFinder::Curve::fill_texture(std::vector<uint32_t>& txt,
                                             const std::vector<eray::math::Vec2f>& params_surface) {
  txt.resize(kTxtSize * kTxtSize, 0xFFFFFFFF);
  constexpr auto kSize = static_cast<float>(kTxtSize);

  auto wins = params_surface | std::views::adjacent<2>;
  for (const auto& [p0, p1] : wins) {
    line_dda(txt, static_cast<int>(p0.x * kSize), static_cast<int>(p0.y * kSize), static_cast<int>(p1.x * kSize),
             static_cast<int>(p1.y * kSize));
  }

  if (!is_closed && points.size() >= 2) {
    // TODO(migoox)
  }
}

void IntersectionFinder::Curve::line_dda(std::vector<uint32_t>& txt, int x0, int y0, int x1, int y1) {
  static const uint32_t kBlack = 0xFF000000;

  int dx = x1 - x0;
  int dy = y1 - y0;

  int steps = std::max(std::abs(dx), std::abs(dy));
  if (steps == 0) {
    return;
  }

  float x_inc = dx / static_cast<float>(steps);
  float y_inc = dy / static_cast<float>(steps);

  float x = x0;
  float y = y0;

  for (int i = 0; i <= steps; ++i) {
    int xi = std::round(x);
    int yi = std::round(y);

    if (xi >= 0 && xi < kTxtSize && yi >= 0 && yi < kTxtSize) {
      txt[yi * kTxtSize + xi] = kBlack;
    }

    x += x_inc;
    y += y_inc;
  }
}

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

static constexpr float wrap_to_unit_interval(float x) noexcept {
  float int_part        = 0.F;
  const float frac_part = std::modf(x, &int_part);
  return frac_part < 0.0F ? frac_part + 1.0F : frac_part;
}

static constexpr math::Vec4f wrap_to_unit_interval(const math::Vec4f& v) noexcept {
  return math::Vec4f(wrap_to_unit_interval(v.x), wrap_to_unit_interval(v.y), wrap_to_unit_interval(v.z),
                     wrap_to_unit_interval(v.w));
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

eray::math::Vec4f IntersectionFinder::newton_start_point_refiner(
    const eray::math::Vec4f& init, PatchSurface& ps1, PatchSurface& ps2, int iters,
    const std::function<float(const eray::math::Vec4f&)>& err_func) {
  constexpr auto kLearningRate = 0.1F;

  auto result = init;
  for (auto i = 0; i < iters; ++i) {
    auto p                   = ps1.evaluate(result.x, result.y);
    const auto& [p_dx, p_dy] = ps1.evaluate_derivatives(result.x, result.y);

    auto q                   = ps2.evaluate(result.z, result.w);
    const auto& [q_dz, q_dw] = ps2.evaluate_derivatives(result.z, result.w);

    auto mat =
        math::Mat4f{-math::Vec4f(p_dx, 0.F), -math::Vec4f(p_dy, 0.F), math::Vec4f(q_dz, 0.F), math::Vec4f(q_dw, 0.F)};

    auto b = math::Vec4f(p - q, 0.F);

    // Test each curve
    auto new_result = math::Vec4f::filled(0.F);
    auto new_delta  = math::Vec4f::filled(0.F);

    bool found = false;
    for (auto j = 0U; j < 4U; ++j) {
      auto curr  = mat;
      curr[j][3] = 1.F;

      if (auto inv = eray::math::inverse(curr)) {
        auto delta = (*inv) * b;

        if (!found || err_func(new_result) > err_func(result + kLearningRate * delta)) {
          new_delta  = kLearningRate * delta;  // why do i multiply by 0.1? idk
          new_result = result + kLearningRate * delta;
          found      = true;
        }
      }
    }

    result = result + new_delta;
    // auto vec1 = p_dx * new_delta.x + p_dy * new_delta.y;
    // auto vec2 = q_dz * new_delta.z + q_dw * new_delta.w;
    // ps1.scene().renderer().debug_line(p, p + vec1);
    // ps1.scene().renderer().debug_line(q, q + vec2);
  }

  return result;
}

eray::math::Vec4f IntersectionFinder::newton_next_point(const eray::math::Vec4f& start, PatchSurface& ps1,
                                                        PatchSurface& ps2, int iters, bool reverse) {
  constexpr auto kLearningRate = 0.1F;

  auto d = 0.1F;

  auto result = start;
  auto p0     = ps1.evaluate(result.x, result.y);

  const auto& [p0_dx, p0_dy] = ps1.evaluate_derivatives(result.x, result.y);
  const auto& [q0_dz, q0_dw] = ps2.evaluate_derivatives(result.z, result.w);

  auto p0n = math::cross(p0_dx, p0_dy);
  auto q0n = math::cross(q0_dz, q0_dw);

  auto t0 = math::normalize(math::cross(p0n, q0n));
  if (reverse) {
    t0 = -t0;
  }

  for (auto i = 0; i < iters; ++i) {
    auto p = ps1.evaluate(result.x, result.y);
    auto q = ps2.evaluate(result.z, result.w);

    const auto& [p_dx, p_dy] = ps1.evaluate_derivatives(result.x, result.y);
    const auto& [q_dz, q_dw] = ps2.evaluate_derivatives(result.z, result.w);

    auto mat = math::Mat4f{-math::Vec4f(p_dx, math::dot(p_dx, t0)), -math::Vec4f(p_dy, math::dot(p_dy, t0)),
                           math::Vec4f(q_dz, 0.F), math::Vec4f(q_dw, 0.F)};

    auto b = math::Vec4f(p - q, math::dot(p, t0) - math::dot(p0, t0) - d);

    if (auto inv = eray::math::inverse(mat)) {
      auto delta = kLearningRate * ((*inv) * b);
      result     = result + delta;
    } else {
      return result;
    }
  }

  return result;
}

std::optional<IntersectionFinder::Curve> IntersectionFinder::find_intersections(PatchSurface& ps1, PatchSurface& ps2) {
  auto bb1 = ps1.aabb_bounding_box();
  auto bb2 = ps2.aabb_bounding_box();
  if (!aabb_intersects(bb1, bb2)) {
    return std::nullopt;
  }

  auto err_func = [&](const math::Vec4f& p) {
    auto diff = ps1.evaluate(p.x, p.y) - ps2.evaluate(p.z, p.w);
    auto res  = math::dot(diff, diff);
    return res;
  };

  auto err_func_grad = [&](const math::Vec4f& p) {
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

  static constexpr auto kLearningRate  = 0.01F;
  static constexpr auto kTolerance     = 0.001F;
  static constexpr auto kMaxIterations = 200;
  static constexpr auto kTrials        = 10;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(0.0F, 1.0F);
  auto random_vec4 = [&]() { return math::Vec4f(dist(gen), dist(gen), dist(gen), dist(gen)); };

  auto start_point =
      gradient_descent(math::Vec4f::filled(0.5F), kLearningRate, kTolerance, kMaxIterations, err_func, err_func_grad);
  for (auto i = 0; i < kTrials; ++i) {
    auto new_result =
        gradient_descent(random_vec4(), kLearningRate, kTolerance, kMaxIterations, err_func, err_func_grad);

    new_result = math::clamp(new_result, 0.F, 1.F);
    if (err_func(start_point) > err_func(new_result)) {
      start_point = new_result;
    }
  }

  eray::util::Logger::info("Found start point with gradient descent: {}, Error: {}", start_point,
                           err_func(start_point));

  {
    auto new_result = newton_start_point_refiner(start_point, ps1, ps2, 5, err_func);
    if (err_func(new_result) < err_func(start_point)) {
      start_point = new_result;
    }
    eray::util::Logger::info("Refined start point with newton method: {}, Error: {}", start_point,
                             err_func(start_point));
  }

  if (err_func(start_point) > kThreshold) {
    return std::nullopt;
  }

  auto curve = Curve{
      .points            = {},            //
      .params_surface1   = {},            //
      .params_surface2   = {},            //
      .surface1          = ps1.handle(),  //
      .surface2          = ps2.handle(),  //
      .txt_params_space1 = {},            //
      .txt_params_space2 = {},            //
      .is_closed         = false,         //
  };
  curve.push_point(start_point, ps1);

  auto is_out_of_unit = [](const math::Vec4f& v) {
    return v.x < 0.F || v.x > 1.F ||  //
           v.y < 0.F || v.y > 1.F ||  //
           v.z < 0.F || v.z > 1.F ||  //
           v.w < 0.F || v.w > 1.F;
  };

  auto next_point = start_point;
  for (auto i = 0U; i < 10000; ++i) {
    next_point   = newton_next_point(next_point, ps1, ps2, 4);
    auto refined = newton_start_point_refiner(next_point, ps1, ps2, 4, err_func);
    if (err_func(refined) < err_func(next_point)) {
      next_point = refined;
    }
    // eray::util::Logger::info("Next point: {}, Error: {}", next_point, err_func(next_point));
    if (is_out_of_unit(next_point)) {
      break;
    }
    curve.push_point(next_point, ps1);
  }

  curve.reverse();

  next_point = start_point;
  for (auto i = 0U; i < 10000; ++i) {
    next_point   = newton_next_point(next_point, ps1, ps2, 4, true);
    auto refined = newton_start_point_refiner(next_point, ps1, ps2, 4, err_func);
    if (err_func(refined) < err_func(next_point)) {
      next_point = refined;
    }
    // eray::util::Logger::info("Next point: {}, Error: {}", next_point, err_func(next_point));
    if (is_out_of_unit(next_point)) {
      break;
    }
    curve.push_point(next_point, ps1);
  }

  if (math::distance(curve.points.front(), curve.points.back()) < 0.1F) {
    curve.points.push_back(curve.points.front());
    curve.is_closed = true;
  }

  curve.fill_textures();

  return curve;
}

}  // namespace mini
