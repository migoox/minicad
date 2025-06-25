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

void IntersectionFinder::Curve::push_point(const eray::math::Vec4f& params, ParamSurface& surface) {
  param_space1.params.emplace_back(params.x, params.y);
  param_space2.params.emplace_back(params.z, params.w);
  points.push_back(surface.eval(params.x, params.y));
}

void IntersectionFinder::Curve::reverse() {
  std::ranges::reverse(points);
  std::ranges::reverse(param_space1.params);
  std::ranges::reverse(param_space2.params);
}

void IntersectionFinder::Curve::fill_textures() {
  draw_curve(param_space1.curve_txt, param_space1.params);
  draw_curve(param_space2.curve_txt, param_space2.params);
  fill_trimming_txts(param_space1.curve_txt, param_space1.trimming_txt1, param_space1.trimming_txt2);
  fill_trimming_txts(param_space2.curve_txt, param_space2.trimming_txt1, param_space2.trimming_txt2);
}

void IntersectionFinder::Curve::draw_curve(std::vector<uint32_t>& txt,
                                           const std::vector<eray::math::Vec2f>& params_surface) {
  txt.resize(kTxtSize * kTxtSize, 0xFFFFFFFF);
  constexpr auto kSize = static_cast<float>(kTxtSize);

  auto wins = params_surface | std::views::adjacent<2>;
  for (const auto& [p0, p1] : wins) {
    line_dda(txt, static_cast<int>(p0.x * kSize), static_cast<int>(p0.y * kSize), static_cast<int>(p1.x * kSize),
             static_cast<int>(p1.y * kSize));
  }
}

void IntersectionFinder::Curve::fill_trimming_txts(const std::vector<uint32_t>& curve_txt, std::vector<uint32_t>& txt1,
                                                   std::vector<uint32_t>& txt2) {
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

  flood_fill(txt1, start_idx % kTxtSize, start_idx / kTxtSize);
  start_idx = 0U;
  for (auto i = 0U; i < kTxtSize * kTxtSize; ++i) {
    if (txt1[i] == kWhite) {
      start_idx = i;
      break;
    }
  }

  flood_fill(txt2, start_idx % kTxtSize, start_idx / kTxtSize);
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

static constexpr float wrap_to_unit_interval(float x) noexcept {
  float int_part        = 0.F;
  const float frac_part = std::modf(x, &int_part);
  return frac_part < 0.0F ? frac_part + 1.0F : frac_part;
}

static constexpr math::Vec4f wrap_to_unit_interval(const math::Vec4f& v) noexcept {
  return math::Vec4f(wrap_to_unit_interval(v.x), wrap_to_unit_interval(v.y), wrap_to_unit_interval(v.z),
                     wrap_to_unit_interval(v.w));
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
    // auto vec1 = p_dx * new_delta.x + p_dy * new_delta.y;
    // auto vec2 = q_dz * new_delta.z + q_dw * new_delta.w;
    // ps1.scene().renderer().debug_line(p, p + vec1);
    // ps1.scene().renderer().debug_line(q, q + vec2);
  }

  return result;
}

eray::math::Vec4f IntersectionFinder::newton_next_point(const float accuracy, const eray::math::Vec4f& start,
                                                        ParamSurface& ps1, ParamSurface& ps2, const int iters,
                                                        const bool reverse) {
  constexpr auto kLearningRate = 0.1F;

  const auto d = accuracy;

  auto result = start;
  auto p0     = ps1.eval(result.x, result.y);

  const auto& [p0_dx, p0_dy] = ps1.evald(result.x, result.y);
  const auto& [q0_dz, q0_dw] = ps2.evald(result.z, result.w);

  auto p0n = math::cross(p0_dx, p0_dy);
  auto q0n = math::cross(q0_dz, q0_dw);

  auto t0 = math::normalize(math::cross(p0n, q0n));
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
      eray::util::Logger::err("Non-singular matrix encountered during newton steps");
      return result;
    }
  }

  return result;
}

std::optional<IntersectionFinder::Curve> IntersectionFinder::find_intersections(ParamSurface& s1, ParamSurface& s2,
                                                                                float accuracy) {
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

  static constexpr auto kLearningRate  = 0.01F;
  static constexpr auto kTolerance     = 0.001F;
  static constexpr auto kMaxIterations = 200;
  static constexpr auto kTrials        = 10;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(0.0F, 1.0F);
  auto random_vec4 = [&]() { return math::Vec4f(dist(gen), dist(gen), dist(gen), dist(gen)); };

  auto start_point = gradient_descent(math::Vec4f::filled(0.5F), kLearningRate, kTolerance, kMaxIterations, err_func);
  for (auto i = 0; i < kTrials; ++i) {
    auto new_result = gradient_descent(random_vec4(), kLearningRate, kTolerance, kMaxIterations, err_func);

    new_result = math::clamp(new_result, 0.F, 1.F);
    if (err_func.eval(start_point) > err_func.eval(new_result)) {
      start_point = new_result;
    }
  }
  eray::util::Logger::info("Start point found with the gradient descent method: {}, Error: {}", start_point,
                           err_func.eval(start_point));

  {
    auto new_result = newton_start_point_refiner(start_point, s1, s2, 5, err_func);
    if (err_func.eval(new_result) < err_func.eval(start_point)) {
      start_point = new_result;
    }
    eray::util::Logger::info("Refined start point with newton method: {}, Error: {}", start_point,
                             err_func.eval(start_point));
  }

  s1.temp_rend.get().debug_point(s1.eval(start_point.x, start_point.y));
  s2.temp_rend.get().debug_point(s2.eval(start_point.z, start_point.w));

  if (err_func.eval(start_point) > kThreshold) {
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

  curve.push_point(start_point, s1);

  auto is_out_of_unit = [&](const math::Vec4f& v) {
    return (!s1.wrap && (v.x < 0.F || v.x > 1.F ||    //
                         v.y < 0.F || v.y > 1.F)) ||  //
           (!s2.wrap && (v.z < 0.F || v.z > 1.F ||    //
                         v.w < 0.F || v.w > 1.F));
  };

  auto is_nan = [&](math::Vec4f& p) {
    return std::isnan(p.x) || std::isnan(p.y) || std::isnan(p.z) || std::isnan(p.w);
  };

  auto refine_point = [&](math::Vec4f& p) {
    auto refined = newton_start_point_refiner(p, s1, s2, 4, err_func);
    if (err_func.eval(refined) < err_func.eval(p)) {
      p = refined;
    }
  };

  auto wrap_if_allowed = [&](math::Vec4f& p) {
    if (s1.wrap) {
      p.x = wrap_to_unit_interval(p.x);
      p.y = wrap_to_unit_interval(p.y);
    }
    if (s2.wrap) {
      p.z = wrap_to_unit_interval(p.z);
      p.w = wrap_to_unit_interval(p.w);
    }
  };

  auto detect_closure = [&](math::Vec4f& first, math::Vec4f& second, math::Vec4f& start) {
    auto f = s1.eval(first.x, first.y);
    auto s = s1.eval(second.x, second.y);
    auto p = s1.eval(start.x, start.y);

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

  auto next_point       = start_point;
  auto end_point        = start_point;
  bool closure_detected = false;
  for (auto i = 0U; i < 10000; ++i) {
    auto prev_point = next_point;
    next_point      = newton_next_point(accuracy, next_point, s1, s2, 8);
    if (is_nan(next_point)) {
      eray::util::Logger::err("NaN encountered");
      break;
    }
    refine_point(next_point);
    wrap_if_allowed(next_point);
    if (is_out_of_unit(next_point)) {
      eray::util::Logger::info("Out of unit circle: {}, Error: {}", next_point, err_func.eval(next_point));
      break;
    }
    if (detect_closure(prev_point, next_point, end_point)) {
      eray::util::Logger::info("Closure detected: {}, Error: {}", next_point, err_func.eval(next_point));
      closure_detected = true;
      curve.push_point(end_point, s1);
      break;
    }
    if (err_func.eval(next_point) > kTolerance) {
      break;
    }
    curve.push_point(next_point, s1);
  }

  if (!closure_detected) {
    curve.reverse();
    end_point  = next_point;
    next_point = start_point;
    for (auto i = 0U; i < 10000; ++i) {
      auto prev_point = next_point;
      next_point      = newton_next_point(accuracy, next_point, s1, s2, 8, true);
      if (is_nan(next_point)) {
        eray::util::Logger::err("NaN encountered");
        break;
      }
      refine_point(next_point);
      wrap_if_allowed(next_point);
      if (is_out_of_unit(next_point)) {
        eray::util::Logger::info("Out of unit circle: {}, Error: {}", next_point, err_func.eval(next_point));
        break;
      }
      if (detect_closure(next_point, prev_point, end_point)) {
        eray::util::Logger::info("Closure detected: {}, Error: {}", next_point, err_func.eval(next_point));
        curve.push_point(end_point, s1);
        break;
      }
      if (err_func.eval(next_point) > kTolerance) {
        break;
      }
      curve.push_point(next_point, s1);
    }
  }

  static constexpr auto kBorderTolerance = 0.01F;
  auto fix_border_closure                = [](eray::math::Vec2f& p) {
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

  curve.fill_textures();

  return curve;
}

}  // namespace mini
