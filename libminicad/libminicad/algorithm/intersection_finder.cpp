#include <algorithm>
#include <cmath>
#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/util/logger.hpp>
#include <libminicad/algorithm/intersection_finder.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene.hpp>
#include <limits>
#include <optional>
#include <random>
#include <ranges>
#include <vector>

namespace mini {

namespace math = eray::math;

static constexpr float wrap_to_unit_interval(float x) noexcept {
  float int_part        = 0.F;
  const float frac_part = std::modf(x, &int_part);
  return frac_part < 0.0F ? frac_part + 1.0F : frac_part;
}

void IntersectionFinder::fix_wrap_flags(ParamSurface& s) {
  if (!s.wrap_u) {
    auto wrap_u        = true;
    const auto samples = 50;
    for (int i = 0; i <= samples; ++i) {
      auto left  = s.eval(0.F, static_cast<float>(i) / static_cast<float>(samples));
      auto right = s.eval(1.F, static_cast<float>(i) / static_cast<float>(samples));

      if (math::distance(left, right) > kWrappingTolerance) {
        wrap_u = false;
        break;
      }
    }
    if (wrap_u) {
      eray::util::Logger::info("Successfully detected that the object's uv might be wrapped along u coord.");
    }
    s.wrap_u = wrap_u;
  }

  if (!s.wrap_v) {
    auto wrap_v        = true;
    const auto samples = 50;
    for (int i = 0; i <= samples; ++i) {
      auto left  = s.eval(static_cast<float>(i) / static_cast<float>(samples), 0.F);
      auto right = s.eval(static_cast<float>(i) / static_cast<float>(samples), 1.F);

      if (math::distance(left, right) > kWrappingTolerance) {
        wrap_v = false;
        break;
      }
    }
    if (wrap_v) {
      eray::util::Logger::info("Successfully detected that the object's uv might be wrapped along v coord.");
    }
    s.wrap_v = wrap_v;
  }
}

void IntersectionFinder::Curve::push_point(const eray::math::Vec4f& params, ParamSurface& s1, ParamSurface& s2) {
  auto uv = params;

  if (s1.wrap_u) {
    uv.x = wrap_to_unit_interval(uv.x);
  }
  if (s1.wrap_v) {
    uv.y = wrap_to_unit_interval(uv.y);
  }
  if (s2.wrap_u) {
    uv.z = wrap_to_unit_interval(uv.z);
  }
  if (s2.wrap_v) {
    uv.w = wrap_to_unit_interval(uv.w);
  }

  if (uv.x > 1.F || uv.x < 0.F || uv.y < 0.F || uv.y > 1.F) {
    uv = eray::math::clamp(uv, 0.F, 1.F);
    points.push_back(s1.eval(uv.x, uv.y));
  } else if (uv.z > 1.F || uv.z < 0.F || uv.w < 0.F || uv.w > 1.F) {
    uv = eray::math::clamp(uv, 0.F, 1.F);
    points.push_back(s2.eval(uv.z, uv.w));
  } else {
    points.push_back((s1.eval(uv.x, uv.y) + s2.eval(uv.z, uv.w)) / 2.F);
  }

  param_space1.params.emplace_back(uv.x, uv.y);
  param_space2.params.emplace_back(uv.z, uv.w);
}

void IntersectionFinder::Curve::reverse() {
  std::ranges::reverse(points);
  std::ranges::reverse(param_space1.params);
  std::ranges::reverse(param_space2.params);
}

void IntersectionFinder::Curve::fill_textures(ParamSurface& s1, ParamSurface& s2) {
  draw_curve(param_space1.curve_txt, param_space1.params);
  draw_curve(param_space2.curve_txt, param_space2.params);
  fill_trimming_txts(param_space1.curve_txt, param_space1.trimming_txt1, param_space1.trimming_txt2, s1);
  fill_trimming_txts(param_space2.curve_txt, param_space2.trimming_txt1, param_space2.trimming_txt2, s2);
}

static eray::math::Vec2f project_to_closest_border(const eray::math::Vec2f& point) {
  float dist_left   = point.x;
  float dist_right  = 1.0F - point.x;
  float dist_top    = 1.0F - point.y;
  float dist_bottom = point.y;

  float min_dist = dist_left;
  math::Vec2f projected(0.0F, point.y);  // Assume left border

  if (dist_right < min_dist) {
    min_dist  = dist_right;
    projected = math::Vec2f{1.0F, point.y};
  }
  if (dist_bottom < min_dist) {
    min_dist  = dist_bottom;
    projected = math::Vec2f{point.x, 0.0F};
  }
  if (dist_top < min_dist) {
    projected = math::Vec2f{point.x, 1.0F};
  }

  return projected;
}

void IntersectionFinder::Curve::draw_curve(std::vector<uint32_t>& txt,
                                           const std::vector<eray::math::Vec2f>& params_surface) {
  txt.resize(kTxtSize * kTxtSize, 0xFFFFFFFF);
  constexpr auto kSize = static_cast<float>(kTxtSize);

  auto wins = params_surface | std::views::adjacent<2>;
  for (const auto& [p0, p1] : wins) {
    auto first = p0;
    auto end   = p1;
    if (math::distance(p0, p1) > 0.2F) {
      end = project_to_closest_border(p0);
    }
    line_dda(txt, static_cast<int>(first.x * kSize), static_cast<int>(first.y * kSize), static_cast<int>(end.x * kSize),
             static_cast<int>(end.y * kSize));
  }

  if (math::distance(params_surface.front(), params_surface.back()) > 0.1F) {
    auto first = params_surface.back();
    auto end   = project_to_closest_border(first);

    if (math::distance(first, end) > 0.2F) {
      end = project_to_closest_border(end);
    }
    line_dda(txt, static_cast<int>(first.x * kSize), static_cast<int>(first.y * kSize), static_cast<int>(end.x * kSize),
             static_cast<int>(end.y * kSize));
  }
}

void IntersectionFinder::Curve::fill_trimming_txts(const std::vector<uint32_t>& curve_txt, std::vector<uint32_t>& txt1,
                                                   std::vector<uint32_t>& txt2, ParamSurface& s) {
  txt1.reserve(curve_txt.size());
  txt2.reserve(curve_txt.size());
  std::ranges::copy(curve_txt, std::back_inserter(txt1));
  std::ranges::copy(curve_txt, std::back_inserter(txt2));

  static constexpr uint32_t kWhite = 0xFFFFFFFF;

  auto start_idx = 0U;
  for (auto i = 0U; i < kTxtSize * kTxtSize; ++i) {
    if (curve_txt[i] == kWhite) {
      start_idx = i;
      break;
    }
  }

  if (curve_txt[kTxtSize / 2 * kTxtSize + kTxtSize / 2] == kWhite) {
    start_idx = kTxtSize / 2 * kTxtSize + kTxtSize / 2;
  }

  flood_fill(txt1, start_idx % kTxtSize, start_idx / kTxtSize);

  for (auto i = 0U; i < kTxtSize * kTxtSize; ++i) {
    if (txt1[i] == kWhite) {
      txt2[i] = 0xFF000000;
    } else {
      txt2[i] = kWhite;
    }
  }
}
void IntersectionFinder::Curve::flood_fill(std::vector<uint32_t>& txt, size_t start_x, size_t start_y) {
  static constexpr uint32_t kBlack = 0xFF000000;

  auto s = std::stack<std::pair<size_t, size_t>>();
  s.emplace(start_x, start_y);

  while (!s.empty()) {
    auto [x, y] = s.top();
    s.pop();
    txt[y * kTxtSize + x] = kBlack;

    if (x + 1 < kTxtSize && txt[y * kTxtSize + x + 1] != kBlack) {
      s.emplace(x + 1, y);
    }
    if (x - 1 < kTxtSize && txt[y * kTxtSize + x - 1] != kBlack) {
      s.emplace(x - 1, y);
    }
    if (y + 1 < kTxtSize && txt[(y + 1) * kTxtSize + x] != kBlack) {
      s.emplace(x, y + 1);
    }
    if (y - 1 < kTxtSize && txt[(y - 1) * kTxtSize + x] != kBlack) {
      s.emplace(x, y - 1);
    }
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

eray::math::Vec4f IntersectionFinder::gradient_descent(const eray::math::Vec4f& init, const float learning_rate,
                                                       const float tolerance, const int max_iters,
                                                       const ErrorFunc& err_func) {
  auto result = init;

  auto prev_val = err_func.eval(result);
  for (auto i = 0; i < max_iters; ++i) {
    auto old_result = result;
    result          = result - learning_rate * err_func.grad(result);
    result.x        = result.x;
    result.y        = result.y;
    result.z        = old_result.z;
    result.w        = old_result.w;

    old_result = result;
    result     = result - learning_rate * err_func.grad(result);
    result.x   = old_result.x;
    result.y   = old_result.y;
    result.z   = result.z;
    result.w   = result.w;

    auto val = err_func.eval(result);

    if (std::abs(prev_val - val) < tolerance) {
      break;
    }
    prev_val = val;
  }

  return result;
}

eray::math::Vec4f IntersectionFinder::newton_start_point_refiner(const eray::math::Vec4f& init, ParamSurface& ps1,
                                                                 ParamSurface& ps2, int iters,
                                                                 const ErrorFunc& err_func) {
  constexpr auto kLearningRate = 0.1F;

  auto result = init;
  for (auto i = 0; i < iters; ++i) {
    auto p                   = ps1.eval(result.x, result.y);
    const auto& [p_dx, p_dy] = ps1.evald(result.x, result.y);

    auto q                   = ps2.eval(result.z, result.w);
    const auto& [q_dz, q_dw] = ps2.evald(result.z, result.w);

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

        if (!found || err_func.eval(new_result) > err_func.eval(result + kLearningRate * delta)) {
          new_delta  = kLearningRate * delta;  // why do i multiply by 0.1? idk
          new_result = result + kLearningRate * delta;
          found      = true;
        }
      }
    }

    result = result + new_delta;
  }

  return result;
}

std::optional<eray::math::Vec4f> IntersectionFinder::newton_next_point(const float accuracy,
                                                                       const eray::math::Vec4f& start,
                                                                       ParamSurface& ps1, ParamSurface& ps2,
                                                                       const int iters, ErrorFunc& err_func,
                                                                       const bool reverse) {
  constexpr auto kLearningRate = 0.1F;

  const auto d = accuracy;

  auto result = start;
  auto p0     = ps1.eval(result.x, result.y);

  const auto& [p0_dx, p0_dy] = ps1.evald(result.x, result.y);
  const auto& [q0_dz, q0_dw] = ps2.evald(result.z, result.w);

  auto p0n = math::normalize(math::cross(p0_dx, p0_dy));
  auto q0n = math::normalize(math::cross(q0_dz, q0_dw));

  auto t0 = math::normalize(math::cross(p0n, q0n));
  ps1.temp_rend.get().debug_line(p0, p0 + p0n);
  ps1.temp_rend.get().debug_line(p0, p0 + q0n);
  if (t0.length() < 0.01F) {
    t0 = math::normalize(p0_dx);
    ps1.temp_rend.get().debug_point(p0);
  }
  if (reverse) {
    t0 = -t0;
  }

  for (auto i = 0; i < iters; ++i) {
    auto p = ps1.eval(result.x, result.y);
    auto q = ps2.eval(result.z, result.w);

    const auto& [p_dx, p_dy] = ps1.evald(result.x, result.y);
    const auto& [q_dz, q_dw] = ps2.evald(result.z, result.w);

    auto mat = math::Mat4f{-math::Vec4f(p_dx, math::dot(p_dx, t0)), -math::Vec4f(p_dy, math::dot(p_dy, t0)),
                           math::Vec4f(q_dz, 0.F), math::Vec4f(q_dw, 0.F)};

    auto b = math::Vec4f(p - q, math::dot(p, t0) - math::dot(p0, t0) - d);

    if (auto inv = eray::math::inverse(mat)) {
      auto delta = kLearningRate * ((*inv) * b);
      result     = result + delta;

    } else {
      return std::nullopt;
    }
  }

  return result;
}

eray::math::Vec4f IntersectionFinder::find_init_point(ParamSurface& s1, ParamSurface& s2, eray::math::Vec3f init) {
  const auto sectors = 20;
  auto min_dist      = std::numeric_limits<float>::max();
  auto result        = eray::math::Vec4f();

  for (auto i = 0; i < sectors; ++i) {
    for (auto j = 0; j < sectors; ++j) {
      auto x = static_cast<float>(i) / static_cast<float>(sectors);
      auto y = static_cast<float>(j) / static_cast<float>(sectors);

      for (auto ii = 0; ii < sectors; ++ii) {
        for (auto jj = 0; jj < sectors; ++jj) {
          auto z = static_cast<float>(ii) / static_cast<float>(sectors);
          auto w = static_cast<float>(jj) / static_cast<float>(sectors);

          auto p1   = s1.eval(x, y);
          auto p2   = s2.eval(z, w);
          auto dist = math::distance(p1, init) + math::distance(p2, init);
          if (dist < min_dist) {
            min_dist = dist;
            result   = eray::math::Vec4f(x, y, z, w);
          }
        }
      }
    }
  }

  return result;
}

std::optional<IntersectionFinder::Curve> IntersectionFinder::find_intersections(ParamSurface& s1, ParamSurface& s2,
                                                                                std::optional<eray::math::Vec3f> init,
                                                                                float accuracy,
                                                                                bool self_intersection) {
  fix_wrap_flags(s1);
  fix_wrap_flags(s2);

  auto is_nan = [&](math::Vec4f& p) {
    return std::isnan(p.x) || std::isnan(p.y) || std::isnan(p.z) || std::isnan(p.w);
  };

  auto err_func_eval = [&](const eray::math::Vec4f& p) {
    auto diff = s1.eval(p.x, p.y) - s2.eval(p.z, p.w);
    auto res  = eray::math::dot(diff, diff);
    return res;
  };

  auto err_func_grad = [&](const eray::math::Vec4f& p) {
    auto diff = s1.eval(p.x, p.y) - s2.eval(p.z, p.w);

    const auto& [ps1_dx, ps1_dy] = s1.evald(p.x, p.y);
    const auto& [ps2_dz, ps2_dw] = s2.evald(p.z, p.w);
    return eray::math::Vec4f{
        2.F * eray::math::dot(ps1_dx, diff),
        2.F * eray::math::dot(ps1_dy, diff),
        -2.F * eray::math::dot(ps2_dz, diff),
        -2.F * eray::math::dot(ps2_dw, diff),
    };
  };

  auto err_func = ErrorFunc{.eval = err_func_eval, .grad = err_func_grad};

  auto is_out_of_unit = [&](const math::Vec4f& v) {
    return (!s1.wrap_u && (v.x < 0.F || v.x > 1.F)) ||  //
           (!s1.wrap_v && (v.y < 0.F || v.y > 1.F)) ||  //
           (!s2.wrap_u && (v.z < 0.F || v.z > 1.F)) ||  //
           (!s2.wrap_v && (v.w < 0.F || v.w > 1.F));
  };

  auto wrap_if_allowed = [&](math::Vec4f& p) {
    if (s1.wrap_u) {
      p.x = wrap_to_unit_interval(p.x);
    }
    if (s1.wrap_v) {
      p.y = wrap_to_unit_interval(p.y);
    }
    if (s2.wrap_u) {
      p.z = wrap_to_unit_interval(p.z);
    }
    if (s2.wrap_v) {
      p.w = wrap_to_unit_interval(p.w);
    }
  };

  auto self_intersection_test = [&](math::Vec4f& p) {
    auto dist = math::distance(math::Vec2f(p.x, p.y), math::Vec2f(p.z, p.w));
    return dist > kSelfIntersectionTolerance;
  };

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(0.0F, 1.0F);
  auto random_vec4 = [&]() { return math::Vec4f(dist(gen), dist(gen), dist(gen), dist(gen)); };

  auto gradient_descent_init_points = std::vector<math::Vec4f>();
  if (self_intersection) {
    const auto sectors = 5;
    gradient_descent_init_points.reserve(sectors * sectors * sectors * sectors);
    for (auto i = 0; i < sectors; ++i) {
      for (auto j = 0; j < sectors; ++j) {
        auto x = static_cast<float>(i) / static_cast<float>(sectors);
        auto y = static_cast<float>(j) / static_cast<float>(sectors);

        for (auto ii = 0; ii < sectors; ++ii) {
          for (auto jj = 0; jj < sectors; ++jj) {
            auto z = static_cast<float>(ii) / static_cast<float>(sectors);
            auto w = static_cast<float>(jj) / static_cast<float>(sectors);

            gradient_descent_init_points.emplace_back(x, y, z, w);
          }
        }
      }
    }
  } else {
    gradient_descent_init_points.reserve(kGradDescTrials);
    for (auto i = 0; i < kGradDescTrials; ++i) {
      gradient_descent_init_points.push_back(random_vec4());
    }
  }

  auto start_point = gradient_descent(math::Vec4f::filled(0.5F), kGradDescLearningRate, kGradDescTolerance,
                                      kGradDescMaxIterations, err_func);
  auto found       = true;
  if (self_intersection) {
    found = false;
  }
  for (const auto& init : gradient_descent_init_points) {
    auto new_result =
        gradient_descent(init, kGradDescLearningRate, kGradDescTolerance, kGradDescMaxIterations, err_func);

    if (is_nan(new_result)) {
      continue;
    }

    if (self_intersection) {
      float dist_new = math::distance(math::Vec2f(new_result.x, new_result.y), math::Vec2f(new_result.z, new_result.w));
      float dist_start =
          math::distance(math::Vec2f(start_point.x, start_point.y), math::Vec2f(start_point.z, start_point.w));

      if (dist_start < dist_new) {
        wrap_if_allowed(new_result);
        if (is_out_of_unit(new_result)) {
          new_result = math::clamp(new_result, 0.F, 1.F);
        }
        start_point = new_result;
        found       = true;
      }
    } else {
      wrap_if_allowed(new_result);
      if (is_out_of_unit(new_result)) {
        new_result = math::clamp(new_result, 0.F, 1.F);
      }
      if (err_func.eval(start_point) > err_func.eval(new_result)) {
        start_point = new_result;
        found       = true;
      }
    }
  }

  if (!found || (self_intersection && !self_intersection_test(start_point))) {
    eray::util::Logger::info("No start point found");
    return std::nullopt;
  }

  eray::util::Logger::info("Start point found with the gradient descent method: {}, Error: {}", start_point,
                           err_func.eval(start_point));

  if (!self_intersection && init) {
    start_point = find_init_point(s1, s2, *init);
    wrap_if_allowed(start_point);
    if (is_out_of_unit(start_point)) {
      start_point = math::clamp(start_point, 0.F, 1.F);
    }
  }

  {
    auto new_result = newton_start_point_refiner(start_point, s1, s2, 100, err_func);
    wrap_if_allowed(new_result);
    new_result = math::clamp(new_result, 0.F, 1.F);
    if (err_func.eval(new_result) < err_func.eval(start_point)) {
      start_point = new_result;
      eray::util::Logger::info("Refined start point with newton refiner: {}, Error: {}", start_point,
                               err_func.eval(start_point));
    }
  }

  if (auto new_result = newton_next_point(accuracy, start_point, s1, s2, 100, err_func)) {
    wrap_if_allowed(*new_result);
    new_result = math::clamp(*new_result, 0.F, 1.F);
    if (err_func.eval(*new_result) < err_func.eval(start_point)) {
      start_point = *new_result;
      eray::util::Logger::info("Refined start point with newton step: {}, Error: {}", start_point,
                               err_func.eval(start_point));
    }
  }

  if (err_func.eval(start_point) > kIntersectionThreshold) {
    eray::util::Logger::info("start point: {}, Error: {} does not satisfy the threshold {}", start_point,
                             err_func.eval(start_point), kIntersectionThreshold);
    return std::nullopt;
  }

  auto curve = Curve{
      .points = {},  //
      .param_space1 =
          {
              .curve_txt     = {},
              .trimming_txt1 = {},
              .trimming_txt2 = {},
              .params        = {},
          },  //
      .param_space2 =
          {
              .curve_txt     = {},
              .trimming_txt1 = {},
              .trimming_txt2 = {},
              .params        = {},
          }  //
  };

  curve.push_point(start_point, s1, s2);

  auto refine_point = [&](math::Vec4f& p) {
    auto refined = newton_start_point_refiner(p, s1, s2, 8, err_func);
    if (err_func.eval(refined) < err_func.eval(p)) {
      p = refined;
    }
  };

  auto detect_closure = [&](math::Vec4f& first, math::Vec4f& second, math::Vec4f& start) {
    auto f = (s1.eval(first.x, first.y) + s2.eval(first.z, first.w)) / 2.F;
    auto s = (s1.eval(second.x, second.y) + s2.eval(second.z, second.w)) / 2.F;
    auto p = (s1.eval(start.x, start.y) + s2.eval(start.z, start.w)) / 2.F;

    auto v1 = p - f;
    auto v2 = p - s;

    const auto& [p0_dx, p0_dy] = s1.evald(start.x, start.y);
    const auto& [q0_dz, q0_dw] = s2.evald(start.z, start.w);

    auto p0n = math::cross(p0_dx, p0_dy);
    auto q0n = math::cross(q0_dz, q0_dw);
    auto t0  = math::normalize(math::cross(p0n, q0n));

    if ((math::dot(t0, v1) > 0) != (math::dot(t0, v2) > 0)) {  // the sign differs => there might be a closure
      if (math::distance(f, p) < 2.F * accuracy || math::distance(s, p) < 2.F * accuracy) {
        return true;
      }
    }

    return false;
  };

  auto try_newton_next_step = [&](const eray::math::Vec4f& p, bool reverse = false) {
    auto curr = std::optional<eray::math::Vec4f>{std::nullopt};

    auto d = accuracy;
    for (auto i = 0; i < 8; ++i) {
      curr = newton_next_point(d, p, s1, s2, 100, err_func, reverse);
      d /= 2.F;

      if (curr != std::nullopt) {
        break;
      }
    }

    return curr;
  };

  auto next_point       = start_point;
  auto end_point        = start_point;
  bool closure_detected = false;
  for (auto i = 0U; i < 10000; ++i) {
    auto prev_point     = next_point;
    auto next_point_opt = try_newton_next_step(next_point);
    if (!next_point_opt) {
      eray::util::Logger::err("Newton iterations failed.");
      break;
    }
    next_point = *next_point_opt;

    if (is_nan(next_point)) {
      eray::util::Logger::err("NaN encountered");
      break;
    }
    refine_point(next_point);
    wrap_if_allowed(next_point);
    if (is_out_of_unit(next_point)) {
      eray::util::Logger::info("Out of unit bounds: {}, Error: {}", next_point, err_func.eval(next_point));
      curve.push_point(next_point, s1, s2);
      break;
    }
    if (i != 0 && detect_closure(prev_point, next_point, end_point)) {
      eray::util::Logger::info("Closure detected: {}, Error: {}", next_point, err_func.eval(next_point));
      closure_detected = true;
      curve.push_point(end_point, s1, s2);
      break;
    }
    if (err_func.eval(next_point) > kGradDescTolerance) {
      break;
    }
    curve.push_point(next_point, s1, s2);
  }

  if (!closure_detected) {
    curve.reverse();
    end_point  = next_point;
    next_point = start_point;
    for (auto i = 0U; i < 10000; ++i) {
      auto prev_point     = next_point;
      auto next_point_opt = try_newton_next_step(next_point, true);
      if (!next_point_opt) {
        eray::util::Logger::err("Newton iterations failed.");
        break;
      }
      next_point = *next_point_opt;

      if (is_nan(next_point)) {
        eray::util::Logger::err("NaN encountered");
        break;
      }
      refine_point(next_point);
      wrap_if_allowed(next_point);
      if (is_out_of_unit(next_point)) {
        eray::util::Logger::info("Out of unit bounds: {}, Error: {}", next_point, err_func.eval(next_point));
        curve.push_point(next_point, s1, s2);
        break;
      }
      if (i != 0 && detect_closure(next_point, prev_point, end_point)) {
        eray::util::Logger::info("Closure detected: {}, Error: {}", next_point, err_func.eval(next_point));
        curve.push_point(end_point, s1, s2);
        closure_detected = true;
        break;
      }
      if (err_func.eval(next_point) > kGradDescTolerance) {
        break;
      }
      curve.push_point(next_point, s1, s2);
    }
  }

  if (!closure_detected) {
    auto fix_border_closure = [](eray::math::Vec2f& p) {
      auto result = false;

      if (p.x < kBorderTolerance) {
        p.x    = 0.F;
        result = true;
      }
      if (std::abs(p.x - 1.F) < kBorderTolerance) {
        p.x    = 1.F;
        result = true;
      }

      if (p.y < kBorderTolerance) {
        p.y    = 0.F;
        result = true;
      }
      if (std::abs(p.y - 1.F) < kBorderTolerance) {
        p.y    = 1.F;
        result = true;
      }

      return result;
    };

    fix_border_closure(curve.param_space1.params.front());
    fix_border_closure(curve.param_space1.params.back());
    fix_border_closure(curve.param_space2.params.front());
    fix_border_closure(curve.param_space2.params.back());
  }

  curve.fill_textures(s1, s2);

  eray::util::Logger::info("Curve point count: {}", curve.points.size());

  return curve;
}

}  // namespace mini
