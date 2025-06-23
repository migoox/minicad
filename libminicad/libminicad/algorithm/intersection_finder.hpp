#pragma once

#include <liberay/math/vec_fwd.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <optional>

namespace mini {

class IntersectionFinder {
 public:
  struct Curve {
    std::vector<eray::math::Vec3f> points;
    std::vector<eray::math::Vec2f> params_surface1;
    std::vector<eray::math::Vec2f> params_surface2;
    PatchSurfaceHandle surface1;
    PatchSurfaceHandle surface2;

    std::vector<uint32_t> txt_params_space1;
    std::vector<uint32_t> txt_params_space2;

    bool is_closed;

    void push_point(const eray::math::Vec4f& params, PatchSurface& surface);
    void reverse();

    void fill_textures();

    static constexpr auto kTxtSize = 512;

   private:
    void fill_texture(std::vector<uint32_t>& txt, const std::vector<eray::math::Vec2f>& params_surface);
    void line_dda(std::vector<uint32_t>& txt, int x0, int y0, int x1, int y1);
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
