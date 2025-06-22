#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/intersection_curve.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>

namespace mini {

IntersectionCurve::IntersectionCurve(IntersectionCurveHandle handle, Scene& scene)
    : ObjectBase<IntersectionCurve, IntersectionCurveVariant>(handle, scene),
      surface1_(PatchSurfaceHandle(0, 0, 0)),
      surface2_(PatchSurfaceHandle(0, 0, 0)) {}

void IntersectionCurve::init(const std::vector<eray::math::Vec3f>& points,
                             const std::vector<eray::math::Vec2f>& param_points_surface1,
                             const std::vector<eray::math::Vec2f>& param_points_surface2,
                             ParametricSurfaceHandle surface1, ParametricSurfaceHandle surface2) {
  surface1_              = surface1;
  surface2_              = surface2;
  param_points_surface1_ = param_points_surface1;
  param_points_surface2_ = param_points_surface2;
  points_                = points;
  this->scene().renderer().push_object_rs_cmd(
      IntersectionCurveRSCommand(handle_, IntersectionCurveRSCommand::Internal::AddObject{}));
}

void IntersectionCurve::update() {}

const std::vector<eray::math::Vec3f>& IntersectionCurve::points() { return points_; }

void IntersectionCurve::clone_to(IntersectionCurve& /*curve*/) const {}

void IntersectionCurve::on_delete() {
  this->scene().renderer().push_object_rs_cmd(
      IntersectionCurveRSCommand(handle_, IntersectionCurveRSCommand::Internal::DeleteObject{}));
}

bool IntersectionCurve::can_be_deleted() const { return true; }

}  // namespace mini
