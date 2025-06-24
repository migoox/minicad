#pragma once

#include <liberay/math/vec_fwd.hpp>
#include <libminicad/scene/handles.hpp>
#include <optional>

namespace mini {

class PatchSurface;

class IntersectionFinder {
 public:
  struct ParamSpace {
    PatchSurfaceHandle surface_handle;
    std::vector<uint32_t> curve_txt;
    std::vector<uint32_t> trimming_txt1;
    std::vector<uint32_t> trimming_txt2;
    std::vector<eray::math::Vec2f> params;
  };

  struct Curve {
    std::vector<eray::math::Vec3f> points;
    ParamSpace param_space1;
    ParamSpace param_space2;

    void push_point(const eray::math::Vec4f& params, PatchSurface& surface);
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
  [[nodiscard]] static std::optional<Curve> find_intersections(PatchSurface& ps1, PatchSurface& ps2);

  static constexpr auto kThreshold = 0.001F;

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
