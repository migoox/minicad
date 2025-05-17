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

bool AddPatchSurfaceModal(zstring_view modal_name, int& x, int& y, bool& cylinder) {
  bool result = false;
  x           = std::max(x, 1);
  y           = std::max(y, 1);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {10.0F, 10.0F});
  if (ImGui::BeginPopupModal(modal_name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputInt("x", &x);
    ImGui::InputInt("y", &y);
    ImGui::Checkbox("Cylinder", &cylinder);
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
