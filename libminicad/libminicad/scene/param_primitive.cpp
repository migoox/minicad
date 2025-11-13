#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/param_primitive.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/trimming.hpp>
#include <libminicad/scene/types.hpp>
#include <limits>
#include <numbers>

namespace mini {
namespace math = eray::math;

eray::math::Mat4f Torus::frenet_frame(const math::Transform3f& transform, const float u, const float v) const {
  auto p        = evaluate(transform, u, v);
  auto [dx, dy] = evaluate_derivatives(transform, u, v);

  dx = math::normalize(dx);
  dy = math::normalize(dy);

  auto norm = math::normalize(math::cross(dx, dy));

  return math::Mat4f{math::Vec4f(dx, 0.F), math::Vec4f(norm, 0.F), math::Vec4f(dy, 0.F), math::Vec4f(p, 1.F)};
}

eray::math::Vec3f Torus::evaluate(const math::Transform3f& transform, float u, float v) const {
  const float x = u * 2.F * std::numbers::pi_v<float>;
  const float y = v * 2.F * std::numbers::pi_v<float>;

  auto pos = transform.local_to_world_matrix() *
             math::Vec4f(std::cos(x) * (minor_radius * std::cos(y) + major_radius), minor_radius * std::sin(y),
                         -std::sin(x) * (minor_radius * std::cos(y) + major_radius), 1.F);
  return eray::math::Vec3f(pos);
}

std::pair<eray::math::Vec3f, eray::math::Vec3f> Torus::evaluate_derivatives(const math::Transform3f& transform,
                                                                            const float u, const float v) const {
  const float x = u * 2.F * std::numbers::pi_v<float>;
  const float y = v * 2.F * std::numbers::pi_v<float>;

  auto scale = transform.scale();

  auto dx = math::Vec3f(                                                     //
      -std::sin(x) * scale.x * (std::cos(y) * minor_radius + major_radius),  //
      0.F,                                                                   //
      -std::cos(x) * scale.z * (std::cos(y) * minor_radius + major_radius)   //
  );

  auto dy = math::Vec3f(                                    //
      -std::sin(y) * std::cos(x) * minor_radius * scale.x,  //
      scale.y * minor_radius * std::cos(y),                 //
      minor_radius * scale.z * std::sin(x) * std::sin(y)    //
  );

  auto rot = transform.rot();

  dx = rot * dx;
  dy = rot * dy;

  return std::make_pair(dx, dy);
}

SecondDerivatives Torus::evaluate_second_derivatives(const eray::math::Transform3f& transform, float u, float v) const {
  eray::util::panic("not implemented yet");  // TODO(migoox): implement

  return SecondDerivatives{};
}

std::pair<eray::math::Vec3f, eray::math::Vec3f> Torus::aabb_bounding_box(const math::Transform3f& /*transform*/) const {
  // TODO(migoox): implement aabb
  eray::util::Logger::warn("AABB bounding box is not implemented for torus.");

  constexpr auto kFltMax = std::numeric_limits<float>::max();
  constexpr auto kFltMin = std::numeric_limits<float>::min();

  return std::make_pair(math::Vec3f::filled(kFltMin), math::Vec3f::filled(kFltMax));
}

ParamPrimitive::ParamPrimitive(ParamPrimitiveHandle handle, Scene& scene)
    : ObjectBase<ParamPrimitive, ParamPrimitiveVariant>(handle, scene),
      trimming_manager_(ParamSpaceTrimmingDataManager::create()),
      txt_handle_(TextureHandle(0, 0, 0)) {
  txt_handle_ = scene_.get().renderer().upload_texture(trimming_manager_.final_txt(),
                                                       ParamSpaceTrimmingDataManager::kGPUTrimmingTxtSize,
                                                       ParamSpaceTrimmingDataManager::kGPUTrimmingTxtSize);
  scene_.get().renderer().push_object_rs_cmd(
      ParamPrimitiveRSCommand(handle_, ParamPrimitiveRSCommand::Internal::AddObject{}));
  scene_.get().renderer().push_object_rs_cmd(
      ParamPrimitiveRSCommand(handle_, ParamPrimitiveRSCommand::UpdateObjectMembers{}));
  scene_.get().renderer().push_object_rs_cmd(
      ParamPrimitiveRSCommand(handle_, ParamPrimitiveRSCommand::Internal::UpdateTrimmingTextures{}));
}

void ParamPrimitive::update() {
  scene().renderer().push_object_rs_cmd(
      ParamPrimitiveRSCommand(handle_, ParamPrimitiveRSCommand::UpdateObjectMembers{}));
}

void ParamPrimitive::on_delete() {
  scene_.get().renderer().push_object_rs_cmd(
      ParamPrimitiveRSCommand(handle_, ParamPrimitiveRSCommand::Internal::DeleteObject{}));
}

bool ParamPrimitive::can_be_deleted() const { return true; }

void ParamPrimitive::clone_to(ParamPrimitive& obj) const {
  obj.transform_ = this->transform_.clone_detached();
  obj.name       = this->name + " Copy";
  obj.object     = this->object;
}

eray::math::Mat4f ParamPrimitive::frenet_frame(float u, float v) {
  return std::visit(
      eray::util::match([&](const CParamPrimitiveType auto& param) { return param.frenet_frame(transform_, u, v); }),
      object);
}

eray::math::Vec3f ParamPrimitive::evaluate(float u, float v) {
  return std::visit(
      eray::util::match([&](const CParamPrimitiveType auto& param) { return param.evaluate(transform_, u, v); }),
      object);
}

std::pair<eray::math::Vec3f, eray::math::Vec3f> ParamPrimitive::evaluate_derivatives(float u, float v) {
  return std::visit(eray::util::match([&](const CParamPrimitiveType auto& param) {
                      return param.evaluate_derivatives(transform_, u, v);
                    }),
                    object);
}

SecondDerivatives ParamPrimitive::evaluate_second_derivatives(float u, float v) {
  return std::visit(eray::util::match([&](const CParamPrimitiveType auto& param) {
                      return param.evaluate_second_derivatives(transform_, u, v);
                    }),
                    object);
}

std::pair<eray::math::Vec3f, eray::math::Vec3f> ParamPrimitive::aabb_bounding_box() {
  return std::visit(
      eray::util::match([&](const CParamPrimitiveType auto& param) { return param.aabb_bounding_box(transform_); }),
      object);
}

void ParamPrimitive::update_trimming_txt() {
  trimming_manager_.update_final_txt();
  scene().renderer().reupload_texture(txt_handle_, trimming_manager_.final_txt_gpu(),
                                      ParamSpaceTrimmingDataManager::kGPUTrimmingTxtSize,
                                      ParamSpaceTrimmingDataManager::kGPUTrimmingTxtSize);
  scene_.get().renderer().push_object_rs_cmd(
      ParamPrimitiveRSCommand(handle_, ParamPrimitiveRSCommand::Internal::UpdateTrimmingTextures{}));
}

}  // namespace mini
