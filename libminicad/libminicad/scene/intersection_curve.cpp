#include <expected>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/intersection_curve.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>

#include "liberay/util/logger.hpp"
#include "liberay/util/object_handle.hpp"
#include "liberay/util/variant_match.hpp"
#include "libminicad/scene/types.hpp"

namespace mini {

IntersectionCurve::IntersectionCurve(IntersectionCurveHandle handle, Scene& scene)
    : ObjectBase<IntersectionCurve, IntersectionCurveVariant>(handle, scene),
      surface1_(PatchSurfaceHandle(0, 0, 0)),
      surface2_(PatchSurfaceHandle(0, 0, 0)),
      txt_surface1_param_space_(0, 0, 0),
      txt_surface2_param_space_(0, 0, 0) {}

std::expected<void, IntersectionCurve::InitError> IntersectionCurve::init(
    const std::vector<eray::math::Vec3f>& points, const std::vector<eray::math::Vec2f>& param_points_surface1,
    const std::vector<eray::math::Vec2f>& param_points_surface2, TextureHandle txt_param_points_surface1,
    TextureHandle txt_param_points_surface2, ParametricSurfaceHandle surface1, ParametricSurfaceHandle surface2) {
  auto param_surface_visitor = [&](const auto& h) {
    using T = ERAY_HANDLE_OBJ(h);
    if (auto opt = scene().arena<T>().get_obj(h)) {
      CParametricSurfaceObject auto& obj = **opt;
      obj.add_intersection_curve(*this);
      return true;
    }
    return false;
  };

  if (!std::visit(eray::util::match{param_surface_visitor}, surface1)) {
    eray::util::Logger::err("Could not init the intersection curve. Parametric Surface does not exist.");
    return std::unexpected(IntersectionCurve::InitError::ParametricSurfaceDoesNotExist);
  }
  if (!std::visit(eray::util::match{param_surface_visitor}, surface2)) {
    eray::util::Logger::err("Could not init the intersection curve. Parametric Surface does not exist.");
    return std::unexpected(IntersectionCurve::InitError::ParametricSurfaceDoesNotExist);
  }

  surface1_                 = surface1;
  surface2_                 = surface2;
  param_points_surface1_    = param_points_surface1;
  param_points_surface2_    = param_points_surface2;
  points_                   = points;
  txt_surface1_param_space_ = txt_param_points_surface1;
  txt_surface2_param_space_ = txt_param_points_surface2;

  this->scene().renderer().push_object_rs_cmd(
      IntersectionCurveRSCommand(handle_, IntersectionCurveRSCommand::Internal::AddObject{}));

  return {};
}

void IntersectionCurve::update() {}

const std::vector<eray::math::Vec3f>& IntersectionCurve::points() { return points_; }

void IntersectionCurve::clone_to(IntersectionCurve& /*curve*/) const {}

void IntersectionCurve::on_delete() {
  auto param_surface_visitor = [&](const auto& h) {
    using T = ERAY_HANDLE_OBJ(h);
    if (auto opt = scene().arena<T>().get_obj(h)) {
      CParametricSurfaceObject auto& obj = **opt;
      obj.remove_intersection_curve(*this);
      return true;
    }
    return false;
  };

  std::visit(eray::util::match{param_surface_visitor}, surface1_);
  std::visit(eray::util::match{param_surface_visitor}, surface2_);

  this->scene().renderer().push_object_rs_cmd(
      IntersectionCurveRSCommand(handle_, IntersectionCurveRSCommand::Internal::DeleteObject{}));
}

bool IntersectionCurve::can_be_deleted() const { return true; }

}  // namespace mini
