#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/intersection_curve.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <libminicad/scene/types.hpp>

namespace mini {

IntersectionCurve::IntersectionCurve(IntersectionCurveHandle handle, Scene& scene)
    : ObjectBase<IntersectionCurve, IntersectionCurveVariant>(handle, scene) {}

std::expected<void, IntersectionCurve::InitError> IntersectionCurve::init(const std::vector<eray::math::Vec3f>& points,
                                                                          const IntersectionCurve::ParamSpace& ps1,
                                                                          const IntersectionCurve::ParamSpace& ps2) {
  auto param_surface_visitor = [&](const auto& h) {
    using T = ERAY_HANDLE_OBJ(h);
    if (auto opt = scene().arena<T>().get_obj(h)) {
      CParametricSurfaceObject auto& obj = **opt;
      obj.add_intersection_curve(*this);
      return true;
    }
    return false;
  };

  if (!std::visit(eray::util::match{param_surface_visitor}, ps1.handle)) {
    eray::util::Logger::err("Could not init the intersection curve. Parametric Surface does not exist.");
    return std::unexpected(IntersectionCurve::InitError::ParametricSurfaceDoesNotExist);
  }
  if (!std::visit(eray::util::match{param_surface_visitor}, ps2.handle)) {
    eray::util::Logger::err("Could not init the intersection curve. Parametric Surface does not exist.");
    return std::unexpected(IntersectionCurve::InitError::ParametricSurfaceDoesNotExist);
  }

  param_space1_ = std::move(ps1);
  param_space2_ = std::move(ps2);

  points_ = points;

  scene().renderer().push_object_rs_cmd(
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

  std::visit(eray::util::match{param_surface_visitor}, param_space1_.handle);
  std::visit(eray::util::match{param_surface_visitor}, param_space2_.handle);

  this->scene().renderer().push_object_rs_cmd(
      IntersectionCurveRSCommand(handle_, IntersectionCurveRSCommand::Internal::DeleteObject{}));
}

bool IntersectionCurve::can_be_deleted() const { return true; }

}  // namespace mini
