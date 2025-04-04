#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imguizmo/ImGuizmo.h>

#include <liberay/math/mat.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/util/enum_mapper.hpp>
#include <liberay/util/logger.hpp>
#include <minicad/imgui/transform_gizmo.hpp>


namespace ImGui {  // NOLINT

namespace mini {

namespace gizmo {

static constexpr auto kImGuizmoMode = eray::util::EnumMapper<Mode, ImGuizmo::MODE>({
    {Mode::Local, ImGuizmo::MODE::LOCAL},
    {Mode::World, ImGuizmo::MODE::WORLD},
});

static constexpr auto kImGuizmoOperation = eray::util::EnumMapper<Operation, ImGuizmo::OPERATION>({
    {Operation::Translation, ImGuizmo::OPERATION::TRANSLATE},
    {Operation::Rotation, ImGuizmo::OPERATION::ROTATE},
    {Operation::Scale, ImGuizmo::OPERATION::SCALE},
});

void SetImGuiContext(ImGuiContext* ctx) { ImGuizmo::SetImGuiContext(ctx); }

void SetRect(float x, float y, eray::math::Vec2f size) { ImGuizmo::SetRect(x, y, size.x, size.y); }

void BeginFrame(ImDrawList* drawlist) {
  ImGuizmo::BeginFrame();
  ImGuizmo::SetDrawlist(drawlist);
}

bool IsOverTransform() { return ImGuizmo::IsOver(); }

bool IsUsingTransform() { return ImGuizmo::IsUsing(); }

bool Transform(eray::math::Transform3f& trans, const ::mini::Camera& camera, Mode mode, Operation operation,
               const std::function<void()>& on_use) {
  static auto old_scale  = eray::math::Vec3f::filled(1.F);
  static auto old_mat    = eray::math::Mat4f::identity();
  static bool is_scaling = false;

  auto view  = camera.view_matrix();
  auto proj  = camera.proj_matrix();
  auto mat   = is_scaling ? old_mat : trans.local_to_world_matrix();
  auto delta = eray::math::Mat4f::identity();

  auto result = false;
  if (ImGuizmo::Manipulate(view.raw_ptr(), proj.raw_ptr(), kImGuizmoOperation[operation], kImGuizmoMode[mode],
                           mat.raw_ptr(), delta.raw_ptr())) {
    on_use();

    if (operation == Operation::Translation) {
      trans.move(eray::math::Vec3f(delta[3]));
    } else if (operation == Operation::Rotation) {
      auto q = eray::math::Quatf::from_rotation_mat(delta);
      trans.rotate_local(q);
    } else if (operation == Operation::Scale) {
      auto scale_vec = eray::math::Vec3f(delta[0][0], delta[1][1], delta[2][2]);
      if (!is_scaling) {
        old_scale = trans.scale();
        old_mat   = trans.local_to_world_matrix();
      }
      auto new_scale = old_scale * scale_vec;
      if (std::abs(eray::math::length(new_scale)) > 0.1F) {
        trans.set_local_scale(new_scale);
      }
    }
    result = true;
  }

  is_scaling = ImGuizmo::IsUsing() && operation == Operation::Scale;

  return result;
}

}  // namespace gizmo

}  // namespace mini

}  // namespace ImGui
