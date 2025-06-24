#pragma once

#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <variant>
#include <vector>

namespace mini {

struct DefaultApproxCurve {
  [[nodiscard]] static zstring_view type_name() noexcept { return "Approx Curve"; }
};

using ApproxCurveVariant = std::variant<DefaultApproxCurve>;

class ApproxCurve : public ObjectBase<ApproxCurve, ApproxCurveVariant> {
 public:
  ApproxCurve() = delete;
  ApproxCurve(ApproxCurveHandle handle, Scene& scene);

  ERAY_DEFAULT_MOVE(ApproxCurve)
  ERAY_DELETE_COPY(ApproxCurve)

  void set_points(const std::vector<eray::math::Vec3f>& points);
  const std::vector<eray::math::Vec3f>& points();

  void update();
  void on_delete();
  void clone_to(ApproxCurve& curve) const;
  bool can_be_deleted() const;

 private:
  std::vector<eray::math::Vec3f> points_;
};

}  // namespace mini
