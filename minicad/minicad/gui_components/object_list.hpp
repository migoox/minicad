#pragma once

#include <imgui/imgui.h>

#include <liberay/util/object_handle.hpp>
#include <libminicad/scene/scene.hpp>

namespace ImGui::mini {  // NOLINT

template <::mini::CObject TObject>
void ObjectListItem(
    const ::mini::Scene& scene, const eray::util::Handle<TObject>& handle, bool is_selected, ImVec2 size = ImVec2(0, 0),
    const std::function<void(const eray::util::Handle<TObject>& handle)>& on_activate =
        [](const eray::util::Handle<TObject>&) {},
    const std::function<void(const eray::util::Handle<TObject>& handle)>& on_deactivate =
        [](const eray::util::Handle<TObject>&) {},
    const std::function<void(const eray::util::Handle<TObject>& handle)>& on_activate_single =
        [](const eray::util::Handle<TObject>&) {},
    const std::function<void(const eray::util::Handle<TObject>& handle)>& on_popup =
        [](const eray::util::Handle<TObject>&) {}) {
  if (auto opt = scene.arena<TObject>().get_obj(handle)) {
    const auto& obj = **opt;
    ImGui::Selectable(obj.name.c_str(), is_selected, ImGuiSelectableFlags_None, size);

    if (ImGui::IsItemHovered()) {
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (is_selected) {
          on_deactivate(handle);
        } else {
          on_activate(handle);
        }
      }

      if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("SelectionPopup");
        on_popup(handle);
      }

      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        on_activate_single(handle);
      }

      if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("SelectionPopup");
      }
    }
  } else {
    ImGui::Selectable("None", false);
  }
}

}  // namespace ImGui::mini
