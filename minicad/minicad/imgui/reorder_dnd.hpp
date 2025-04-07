#pragma once

#include <libminicad/scene/scene_object.hpp>

namespace ImGui::mini {  // NOLINT

struct ReorderDnD {
 public:
  ReorderDnD() = delete;
  explicit ReorderDnD(zstring_view payload_type) : payload_type_(payload_type) {}

  void drag_and_drop_component(const ::mini::SceneObject& obj, bool ignore_middle);

 public:
  std::optional<::mini::SceneObjectHandle> source;
  std::optional<::mini::SceneObjectHandle> before_dest;
  std::optional<::mini::SceneObjectHandle> middle_dest;
  std::optional<::mini::SceneObjectHandle> after_dest;

 private:
  zstring_view payload_type_;
};

}  // namespace ImGui::mini
