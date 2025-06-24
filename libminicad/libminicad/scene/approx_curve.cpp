#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/approx_curve.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <libminicad/scene/types.hpp>

namespace mini {

ApproxCurve::ApproxCurve(ApproxCurveHandle handle, Scene& scene)
    : ObjectBase<ApproxCurve, ApproxCurveVariant>(handle, scene) {}

void ApproxCurve::set_points(const std::vector<eray::math::Vec3f>& points) {
  points_ = points;
  scene().renderer().push_object_rs_cmd(ApproxCurveRSCommand(handle_, ApproxCurveRSCommand::Internal::AddObject{}));
}

void ApproxCurve::update() {}

const std::vector<eray::math::Vec3f>& ApproxCurve::points() { return points_; }

void ApproxCurve::clone_to(ApproxCurve& /*curve*/) const {}

void ApproxCurve::on_delete() {
  this->scene().renderer().push_object_rs_cmd(
      ApproxCurveRSCommand(handle_, ApproxCurveRSCommand::Internal::DeleteObject{}));
}

bool ApproxCurve::can_be_deleted() const { return true; }

}  // namespace mini
