#pragma once

#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <variant>
#include <vector>

namespace mini {

struct TrimmingIntersectionCurve {
  [[nodiscard]] static zstring_view type_name() noexcept { return "Trimming Curve"; }
};

using IntersectionCurveVariant = std::variant<TrimmingIntersectionCurve>;

class IntersectionCurve : public ObjectBase<IntersectionCurve, IntersectionCurveVariant> {
 public:
  IntersectionCurve() = delete;
  IntersectionCurve(IntersectionCurveHandle handle, Scene& scene);

  ERAY_DEFAULT_MOVE(IntersectionCurve)
  ERAY_DELETE_COPY(IntersectionCurve)

  enum class InitError : uint8_t { ParametricSurfaceDoesNotExist = 0 };

  std::expected<void, InitError> init(const std::vector<eray::math::Vec3f>& points,
                                      const std::vector<eray::math::Vec2f>& param_points_surface1,
                                      const std::vector<eray::math::Vec2f>& param_points_surface2,
                                      TextureHandle txt_param_points_surface1, TextureHandle txt_param_points_surface2,
                                      ParametricSurfaceHandle surface1, ParametricSurfaceHandle surface2);

  const std::vector<eray::math::Vec3f>& points();

  void update();
  void on_delete();
  void clone_to(IntersectionCurve& curve) const;
  bool can_be_deleted() const;

 private:
  ParametricSurfaceHandle surface1_;
  ParametricSurfaceHandle surface2_;
  TextureHandle txt_surface1_param_space_;
  TextureHandle txt_surface2_param_space_;
  std::vector<eray::math::Vec2f> param_points_surface1_;
  std::vector<eray::math::Vec2f> param_points_surface2_;
  std::vector<eray::math::Vec3f> points_;
  bool is_closed_;
};

}  // namespace mini
