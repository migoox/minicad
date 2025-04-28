#include <imgui/imgui.h>

#include <liberay/math/transform3.hpp>
#include <minicad/imgui/transform.hpp>

namespace ImGui::mini {  // NOLINT

namespace math = eray::math;

bool Transform(eray::math::Transform3f& trans, const std::function<void()>& on_use) {
  auto pos = trans.pos();
  ImGui::InputFloat3("Pos", pos.raw_ptr());
  auto result = false;
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    trans.set_local_pos(pos);
    result = true;
  }

  auto rot    = trans.rot();
  auto eulers = math::degrees(math::eulers_xyz(rot.rot_mat()));
  ImGui::InputFloat3("Rot", eulers.raw_ptr());
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    auto reulers = math::radians(eulers);
    trans.set_local_rot(math::Quatf::from_euler_xyz(reulers));
    result = true;
  }

  auto scale = trans.scale();
  ImGui::InputFloat3("Scale", scale.raw_ptr());
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    trans.set_local_scale(scale);
    result = true;
  }

  if (result) {
    on_use();
  }

  return result;
}

}  // namespace ImGui::mini
