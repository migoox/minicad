#pragma once

#include <liberay/math/vec_fwd.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/handles.hpp>
#include <libminicad/scene/types.hpp>
#include <optional>

namespace mini {

class PatchSurface;

class IntersectionFinder {
 public:
  struct ParamSurface {
    ref<ISceneRenderer> temp_rend;
    bool wrap_u = false;
    bool wrap_v = false;
    std::function<eray::math::Vec3f(float, float)> eval;
    std::function<std::pair<eray::math::Vec3f, eray::math::Vec3f>(float, float)> evald;
  };

  struct ParamSpace {
    std::vector<uint32_t> curve_txt;
    std::vector<uint32_t> trimming_txt1;
    std::vector<uint32_t> trimming_txt2;
    std::vector<eray::math::Vec2f> params;
  };

  struct Curve {
    std::vector<eray::math::Vec3f> points;
    ParamSpace param_space1;
    ParamSpace param_space2;

    void push_point(const eray::math::Vec4f& params, ParamSurface& s1, ParamSurface& s2);
    void reverse();

    void fill_textures();

    static constexpr auto kTxtSize = 512;

   private:
    void draw_curve(std::vector<uint32_t>& txt, const std::vector<eray::math::Vec2f>& params_surface);
    void line_dda(std::vector<uint32_t>& txt, int x0, int y0, int x1, int y1);
    void fill_trimming_txts(const std::vector<uint32_t>& curve_txt, std::vector<uint32_t>& txt1,
                            std::vector<uint32_t>& txt2);
    void flood_fill(std::vector<uint32_t>& txt, size_t start_x, size_t start_y);
  };

  /**
   * @brief Finds intersection between two parametric surfaces. Returns nullopt if no intersections are found.
   *
   * @param h1
   * @param h2
   * @return std::optional<Result>
   */
  template <CParametricSurfaceObject T1, CParametricSurfaceObject T2>
  [[nodiscard]] static std::optional<Curve> find_intersections(ISceneRenderer& renderer, T1& ps1, T2& ps2,
                                                               float accuracy = 0.1F) {
    auto bb1 = ps1.aabb_bounding_box();
    auto bb2 = ps2.aabb_bounding_box();
    if (!aabb_intersects(bb1, bb2)) {
      return std::nullopt;
    }

    auto eval1  = [&](float u, float v) { return ps1.evaluate(u, v); };
    auto evald1 = [&](float u, float v) { return ps1.evaluate_derivatives(u, v); };

    auto eval2  = [&](float u, float v) { return ps2.evaluate(u, v); };
    auto evald2 = [&](float u, float v) { return ps2.evaluate_derivatives(u, v); };

    auto wrap1 = false;
    auto wrap2 = false;

    if constexpr (std::is_same_v<T1, ParamPrimitive>) {
      wrap1 = true;
    }
    if constexpr (std::is_same_v<T2, ParamPrimitive>) {
      wrap2 = true;
    }

    auto s1 = ParamSurface{
        .temp_rend = renderer,
        .wrap_u    = wrap1,
        .wrap_v    = wrap1,
        .eval      = std::move(eval1),
        .evald     = std::move(evald1),
    };
    auto s2 = ParamSurface{
        .temp_rend = renderer,
        .wrap_u    = wrap2,
        .wrap_v    = wrap2,
        .eval      = std::move(eval2),
        .evald     = std::move(evald2),
    };

    return find_intersections(s1, s2, accuracy);
  }

  static std::optional<Curve> find_intersections(ParamSurface& s1, ParamSurface& s2, float accuracy = 0.1F);

  static constexpr auto kIntersectionThreshold = 0.1F;

  static constexpr auto kGradDescLearningRate  = 0.01F;
  static constexpr auto kGradDescTolerance     = 0.00001F;
  static constexpr auto kGradDescMaxIterations = 400;
  static constexpr auto kGradDescTrials        = 100;

  static constexpr auto kBorderTolerance = 0.01F;

  static constexpr auto kWrappingTolerance = 0.01F;

 private:
  struct ErrorFunc {
    std::function<float(const eray::math::Vec4f&)> eval;
    std::function<eray::math::Vec4f(const eray::math::Vec4f&)> grad;
  };

  static void fix_wrap_flags(ParamSurface& s);

  static bool aabb_intersects(const std::pair<eray::math::Vec3f, eray::math::Vec3f>& a,
                              const std::pair<eray::math::Vec3f, eray::math::Vec3f>& b);

  static eray::math::Vec4f gradient_descent(const eray::math::Vec4f& init, float learning_rate, float tolerance,
                                            int max_iters, const ErrorFunc& err_func);
  static eray::math::Vec4f newton_start_point_refiner(const eray::math::Vec4f& init, ParamSurface& ps1,
                                                      ParamSurface& ps2, int iters, const ErrorFunc& err_func);

  static eray::math::Vec4f newton_next_point(float accuracy, const eray::math::Vec4f& start, ParamSurface& ps1,
                                             ParamSurface& ps2, int iters, ErrorFunc& err_func, bool reverse = false);
};

}  // namespace mini
