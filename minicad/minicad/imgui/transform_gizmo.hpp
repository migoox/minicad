#pragma once

#include <imgui/imgui.h>

#include <cstdint>
#include <liberay/math/transform3.hpp>
#include <libminicad/camera/camera.hpp>

namespace ImGui {  // NOLINT

namespace mini {

namespace gizmo {

enum class Mode : uint8_t { Local = 0, World = 1, _Count = 2 };                           // NOLINT
enum class Operation : uint8_t { Translation = 0, Rotation = 1, Scale = 2, _Count = 3 };  // NOLINT

void SetImGuiContext(ImGuiContext* ctx);
void BeginFrame(ImDrawList* drawlist = nullptr);
bool IsTransformUsed();
bool Transform(eray::math::Transform3f& trans, const ::mini::Camera& camera, Mode mode, Operation operation,
               bool freeze = false);

}  // namespace gizmo

}  // namespace mini

}  // namespace ImGui
