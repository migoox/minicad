#pragma once

#include <libminicad/scene/scene_object.hpp>

namespace ImGui::mini {  // NOLINT

struct ReorderDnD {
 public:
  ReorderDnD() = delete;
  explicit ReorderDnD(zstring_view payload_type) : payload_type_(payload_type) {}

  void drag_and_drop_component(const ::mini::SceneObject& obj, size_t idx, bool ignore_middle);

 public:
  std::optional<size_t> source;
  std::optional<size_t> before_dest;
  std::optional<size_t> middle_dest;
  std::optional<size_t> after_dest;

 private:
  zstring_view payload_type_;
};

}  // namespace ImGui::mini
