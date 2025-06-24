#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imgui_stdlib.h>

#include <algorithm>
#include <minicad/imgui/modals.hpp>
#include <string>

namespace ImGui {

namespace mini {
static bool modal_start = false;  // NOLINT

void OpenModal(zstring_view modal_name) {
  ImGui::OpenPopup(modal_name.c_str());
  modal_start = true;
}

bool RenameModal(zstring_view modal_name, std::string& name_holder) {
  static bool init_selection = false;
  bool result                = false;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {10.0F, 10.0F});
  if (ImGui::BeginPopupModal(modal_name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (modal_start) {
      ImGui::SetKeyboardFocusHere();
      init_selection = true;
      modal_start    = false;
    }

    ImGui::PushItemWidth(250.0F);
    if (ImGui::InputText("##Name", &name_holder) || ImGui::IsKeyDown(ImGuiKey_LeftArrow) ||
        ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
      init_selection = false;
    }

    if (ImGui::IsItemDeactivatedAfterEdit() && ImGui::IsKeyDown(ImGuiKey_Enter)) {
      if (ImGui::IsKeyDown(ImGuiKey_Enter)) {
        if (!name_holder.empty()) {
          ImGui::CloseCurrentPopup();
          result = true;
        }
      }
    }

    ImGui::PopItemWidth();
    if (init_selection) {
      if (ImGuiInputTextState * state{ImGui::GetInputTextState(ImGui::GetItemID())}) {
        state->ReloadUserBufAndSelectAll();
      }
    }

    if (name_holder.empty()) {
      ImGui::BeginDisabled();
    }

    ImGui::SetItemDefaultFocus();
    if (ImGui::Button("Rename", ImVec2(120, 0))) {
      if (!name_holder.empty()) {
        ImGui::CloseCurrentPopup();
        result = true;
      }
    }

    if (name_holder.empty()) {
      ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();
  return result;
}

bool NaturalSplineModal(zstring_view modal_name, int& control_points) {
  bool result = false;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {10.0F, 10.0F});
  if (ImGui::BeginPopupModal(modal_name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    control_points = std::max(control_points, 3);

    ImGui::InputInt("Points", &control_points);

    if (ImGui::Button("Create", ImVec2(120, 0))) {
      result = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();
  return result;
}

bool MessageOkCancelModal(zstring_view modal_name, zstring_view msg, zstring_view ok_button_label,
                          zstring_view cancel_button_label) {
  bool result = false;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {10.0F, 10.0F});
  if (ImGui::BeginPopupModal(modal_name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("%s", msg.c_str());
    if (ImGui::Button(ok_button_label.c_str(), ImVec2(120, 0))) {
      result = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(cancel_button_label.c_str(), ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();
  return result;
}

bool AddPatchSurfaceModal(zstring_view modal_name, PatchSurfaceInfo& info, bool cylinder_from_curve) {
  bool result = false;
  info.x      = std::max(info.x, 1);
  info.y      = std::max(info.y, 1);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {10.0F, 10.0F});
  if (ImGui::BeginPopupModal(modal_name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (info.cylinder || cylinder_from_curve) {
      info.x = std::max(info.x, 3);
    }
    ImGui::InputInt("X", &info.x);
    ImGui::InputInt("Y", &info.y);
    if (!cylinder_from_curve) {
      ImGui::Checkbox("Cylinder", &info.cylinder);
    }
    if (!info.cylinder && !cylinder_from_curve) {
      ImGui::DragFloat("Size X", &info.size_x, 0.1F, 1.F, 50.F);
      ImGui::DragFloat("Size Y", &info.size_y, 0.1F, 1.F, 50.F);
    } else {
      ImGui::DragFloat("Radius", &info.r);
      ImGui::DragFloat("Phase (degrees)", &info.phase);
      if (!cylinder_from_curve) {
        ImGui::DragFloat("Height", &info.h);
      }
    }
    if (ImGui::Button("Add", ImVec2(120, 0))) {
      result = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();
  return result;
}

}  // namespace mini

}  // namespace ImGui
