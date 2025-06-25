#pragma once

#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/trimming.hpp>
#include <libminicad/scene/types.hpp>

namespace mini {
class Torus {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Torus"; }

 public:
  float minor_radius           = 1.F;
  float major_radius           = 2.F;
  eray::math::Vec2i tess_level = eray::math::Vec2i(16, 16);
};

template <typename T>
concept CParamPrimitiveType = requires {
  { T::type_name() } -> std::same_as<zstring_view>;
};

using ParamPrimitiveVariant = std::variant<Torus>;
MINI_VALIDATE_VARIANT_TYPES(ParamPrimitiveVariant, CParamPrimitiveType);

class ParamPrimitive : public ObjectBase<ParamPrimitive, ParamPrimitiveVariant> {
 public:
  ParamPrimitive() = delete;
  ParamPrimitive(ParamPrimitiveHandle handle, Scene& scene);

  ERAY_DEFAULT_MOVE(ParamPrimitive)
  ERAY_DELETE_COPY(ParamPrimitive)

  void update();
  void on_delete();
  bool can_be_deleted() const;
  void clone_to(ParamPrimitive& obj) const;

  eray::math::Mat4f frenet_frame(float u, float v);
  eray::math::Vec3f evaluate(float u, float v);
  std::pair<eray::math::Vec3f, eray::math::Vec3f> evaluate_derivatives(float u, float v);
  std::pair<eray::math::Vec3f, eray::math::Vec3f> aabb_bounding_box();

  ParamSpaceTrimmingDataManager& trimming_manager() { return trimming_manager_; }
  const ParamSpaceTrimmingDataManager& trimming_manager() const { return trimming_manager_; }

  void update_trimming_txt();
  const TextureHandle& txt_handle() const { return txt_handle_; }

  eray::math::Transform3f& transform() { return transform_; }
  const eray::math::Transform3f& transform() const { return transform_; }

 private:
  eray::math::Transform3f transform_;
  ParamSpaceTrimmingDataManager trimming_manager_;
  TextureHandle txt_handle_;
};

static_assert(CObject<ParamPrimitive>);
static_assert(CParametricSurfaceObject<ParamPrimitive>);
static_assert(CTransformableObject<ParamPrimitive>);

}  // namespace mini
