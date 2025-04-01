#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imguizmo/ImGuizmo.h>

#include <liberay/math/mat.hpp>
#include <minicad/imgui/transform_gizmo.hpp>

#include "liberay/math/quat.hpp"

namespace ImGui {  // NOLINT

namespace mini {

namespace gizmo {

void SetImGuiContext(ImGuiContext* ctx) { ImGuizmo::SetImGuiContext(ctx); }

void BeginFrame(ImDrawList* drawlist) {
  ImGuizmo::BeginFrame();
  ImGuizmo::SetDrawlist(drawlist);
}

bool IsTransformUsed() { return ImGuizmo::IsUsing(); }

bool Transform(eray::math::Transform3f& trans, const ::mini::Camera& camera, Mode mode, Operation operation,
               bool freeze) {
  using vec3 = eray::math::Vec3f;
  using vec4 = eray::math::Vec4f;
  using mat4 = eray::math::Mat4f;
  using quat = eray::math::Quatf;

  const float pos_x  = ImGui::GetWindowPos().x + ImGui::GetCursorStartPos().x;
  const float pos_y  = ImGui::GetWindowPos().y + ImGui::GetCursorStartPos().y;
  const float width  = ImGui::GetWindowWidth() - ImGui::GetCursorStartPos().x;
  const float height = ImGui::GetWindowHeight() - ImGui::GetCursorStartPos().y;

  ImGuizmo::SetOrthographic(camera.is_orthographic());
  ImGuizmo::SetRect(pos_x, pos_y, width, height);

  auto view      = camera.view_matrix();
  auto proj      = camera.proj_matrix();
  auto delta_mat = mat4::identity();

  auto mat = trans.local_to_world_matrix();

  if (operation == Operation::Translation) {
    if (ImGuizmo::Manipulate(view.raw_ptr(), proj.raw_ptr(), ImGuizmo::OPERATION::TRANSLATE,
                             mode == Mode::Local ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD, mat.raw_ptr(),
                             delta_mat.raw_ptr()) &&
        !freeze) {
      auto dp = vec3(delta_mat[3]);
      if (trans.has_parent()) {
        auto parent_rot_mat = trans.parent().rot().rot_mat();
        dp                  = vec3(eray::math::transpose(parent_rot_mat) * vec4(dp.x, dp.y, dp.z, 1.0F));
      }
      trans.move(dp);

      return true;
    }
    return false;
  }

  if (operation == Operation::Rotation) {
    if (ImGuizmo::Manipulate(view.raw_ptr(), proj.raw_ptr(), ImGuizmo::OPERATION::ROTATE,
                             mode == Mode::Local ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD, mat.raw_ptr(),
                             delta_mat.raw_ptr()) &&
        !freeze) {
      auto dq = quat::from_rotation_mat(delta_mat);
      if (trans.has_parent()) {
        auto pq = trans.parent().rot();

        // new world rotation:
        //
        //      q_new = dq * pq * q,
        //
        // where q is the current local rotation of the object, pq is the parent
        // world rotation and dq is delta obtained from the ImGuizmo
        //
        // however we can only influence the q, so we need dq' that could be
        // plugged in like this:
        //
        //      q_new = pq * dq' * q
        //
        // from dq * pq * q = pq * dq' * q, we get
        //
        //      dq' = pq^{-1} * dq * pq

        dq = eray::math::normalize(eray::math::inverse(pq) * dq * pq);
      }
      trans.rotate(dq);
      return true;
    }
    return false;
  }

  //   if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), ImGuizmo::OPERATION::SCALE,
  //                            ImGuizmo::MODE::WORLD, glm::value_ptr(mat), glm::value_ptr(delta_mat)) &&
  //       !freeze) {
  //     constexpr float kFloatEqTreshold = 1e-5F;

  //     // Note: delta matrix doesn't contain a delta per frame, but a delta from the moment the user grabbed the gizmo
  //     // for the first time so we can't use it here ☹️
  //     float scale_fix = 1.0F;
  //     if (trans.has_parent()) {
  //       scale_fix = trans.parent().scale();
  //     }

  //     // Assuming uniform scaling
  //     if (std::abs(mat[1][1] - mat[0][0]) > kFloatEqTreshold) {
  //       if (std::abs(mat[1][1] - mat[2][2]) > kFloatEqTreshold) {
  //         trans.set_local_scale(mat[1][1] / scale_fix);
  //       } else {
  //         trans.set_local_scale(mat[0][0] / scale_fix);
  //       }
  //     } else if (std::abs(mat[2][2] - mat[0][0]) > kFloatEqTreshold) {
  //       trans.set_local_scale(mat[2][2] / scale_fix);
  //     } else if (std::abs(mat[0][0] - trans.scale()) > kFloatEqTreshold) {
  //       // handle the case when origin is used for scaling
  //       trans.set_local_scale(mat[0][0] / scale_fix);
  //     }

  //     return true;
  //   }

  return false;
}

}  // namespace gizmo

}  // namespace mini

}  // namespace ImGui
