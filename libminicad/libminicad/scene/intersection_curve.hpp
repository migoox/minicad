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

  struct ParamSpace {
    ParametricSurfaceHandle handle = PatchSurfaceHandle(0, 0, 0);
    TextureHandle curve_txt        = TextureHandle(0, 0, 0);
    TextureHandle trimming_txt1    = TextureHandle(0, 0, 0);
    TextureHandle trimming_txt2    = TextureHandle(0, 0, 0);
    std::vector<eray::math::Vec2f> params;
  };

  ERAY_DEFAULT_MOVE(IntersectionCurve)
  ERAY_DELETE_COPY(IntersectionCurve)

  enum class InitError : uint8_t { ParametricSurfaceDoesNotExist = 0 };

  std::expected<void, InitError> init(const std::vector<eray::math::Vec3f>& points, const ParamSpace& ps1,
                                      const ParamSpace& ps2);

  const ParamSpace& param_spaces1() { return param_space1_; }
  const ParamSpace& param_spaces2() { return param_space2_; }

  const std::vector<eray::math::Vec3f>& points();

  void update();
  void on_delete();
  void clone_to(IntersectionCurve& curve) const;
  bool can_be_deleted() const;

 private:
  ParamSpace param_space1_;
  ParamSpace param_space2_;

  std::vector<eray::math::Vec2f> param_points_surface1_;
  std::vector<eray::math::Vec2f> param_points_surface2_;
  std::vector<eray::math::Vec3f> points_;
};

}  // namespace mini
