#include <imgui/imgui.h>

#include <minicad/imgui/reorder_dnd.hpp>

namespace ImGui::mini {

void mini::ReorderDnD::drag_and_drop_component(const ::mini::SceneObject& obj, const size_t idx, bool ignore_middle) {
  if (ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload(payload_type_.c_str(), &idx, sizeof(size_t));
    ImGui::Text("%s", obj.name.c_str());
    ImGui::EndDragDropSource();
  }

  auto dnd_col = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
  if (ImGui::BeginDragDropTarget()) {
    ImVec2 item_min = ImGui::GetItemRectMin();
    ImVec2 item_max = ImGui::GetItemRectMax();

    float bottom_dist = item_max.y - ImGui::GetMousePos().y;
    bool middle       = !ignore_middle && ImGui::GetItemRectSize().y / 3.F * 2.F > bottom_dist &&
                  ImGui::GetItemRectSize().y / 3.F < bottom_dist;
    bool below = ImGui::GetItemRectSize().y / 2.F > bottom_dist;

    if (middle) {
      ImGui::GetForegroundDrawList()->AddRect(item_min, item_max, dnd_col);
    } else if (below) {
      ImGui::GetForegroundDrawList()->AddLine(ImVec2(item_min.x, item_max.y), item_max, dnd_col);
    } else {
      ImGui::GetForegroundDrawList()->AddLine(item_min, ImVec2(item_max.x, item_min.y), dnd_col);
    }

    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(payload_type_.c_str(), ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
      IM_ASSERT(payload->DataSize == sizeof(size_t));

      source = *static_cast<size_t*>(payload->Data);
      if (middle) {
        middle_dest = idx;
      } else if (below) {
        after_dest = idx;
      } else {
        before_dest = idx;
      }
    }

    ImGui::EndDragDropTarget();
  }
}
}  // namespace ImGui::mini
