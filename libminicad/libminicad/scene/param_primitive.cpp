#include <liberay/util/logger.hpp>
#include <libminicad/scene/param_primitive.hpp>
#include <libminicad/scene/scene.hpp>

namespace mini {

ParamPrimitive::ParamPrimitive(ParamPrimitiveHandle handle, Scene& scene)
    : ObjectBase<ParamPrimitive, ParamPrimitiveVariant>(handle, scene),
      trimming_manager_(ParamSpaceTrimmingDataManager::create(IntersectionFinder::Curve::kTxtSize,
                                                              IntersectionFinder::Curve::kTxtSize)),
      txt_handle_(TextureHandle(0, 0, 0)) {
  txt_handle_ = scene_.get().renderer().upload_texture(trimming_manager_.final_txt(), trimming_manager_.width(),
                                                       trimming_manager_.height());
  // TODO(migoox): push rs command
}

void ParamPrimitive::update() {}

void ParamPrimitive::on_delete() {
  // TODO(migoox): push rs command
}

bool ParamPrimitive::can_be_deleted() const { return true; }

void ParamPrimitive::clone_to(ParamPrimitive& obj) const {
  obj.transform_ = this->transform_.clone_detached();
  obj.name       = this->name + " Copy";
  obj.object     = this->object;
}

eray::math::Mat4f ParamPrimitive::frenet_frame(float u, float v) {
  eray::util::Logger::err("Not implemented!");
  return {};
}

eray::math::Vec3f ParamPrimitive::evaluate(float u, float v) {
  eray::util::Logger::err("Not implemented!");
  return {};
}

std::pair<eray::math::Vec3f, eray::math::Vec3f> ParamPrimitive::evaluate_derivatives(float u, float v) {
  eray::util::Logger::err("Not implemented!");
  return {};
}

std::pair<eray::math::Vec3f, eray::math::Vec3f> ParamPrimitive::aabb_bounding_box() {
  eray::util::Logger::err("Not implemented!");
  return {};
}

void ParamPrimitive::update_trimming_txt() {
  trimming_manager_.update_final_txt();
  scene().renderer().reupload_texture(txt_handle_, trimming_manager_.final_txt(), trimming_manager_.width(),
                                      trimming_manager_.height());
  // TODO(migoox): push rs command
}

}  // namespace mini
