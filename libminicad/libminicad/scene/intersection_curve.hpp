#pragma once

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

  void init(const std::vector<eray::math::Vec3f>& points, const std::vector<eray::math::Vec2f>& param_points_surface1,
            const std::vector<eray::math::Vec2f>& param_points_surface2, ParametricSurfaceHandle surface1,
            ParametricSurfaceHandle surface2);

  const std::vector<eray::math::Vec3f>& points();

  void update();
  void on_delete();
  void clone_to(IntersectionCurve& curve) const;
  bool can_be_deleted() const;

 private:
  ParametricSurfaceHandle surface1_;
  ParametricSurfaceHandle surface2_;
  std::vector<eray::math::Vec2f> param_points_surface1_;
  std::vector<eray::math::Vec2f> param_points_surface2_;
  std::vector<eray::math::Vec3f> points_;
};

}  // namespace mini
