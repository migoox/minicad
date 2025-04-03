#pragma once

#include <imgui/imgui.h>

#include <cstdint>
#include <functional>
#include <liberay/math/transform3.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/camera/camera.hpp>

namespace ImGui {  // NOLINT

namespace mini {

namespace gizmo {

enum class Mode : uint8_t { Local = 0, World = 1, _Count = 2 };                           // NOLINT
enum class Operation : uint8_t { Translation = 0, Rotation = 1, Scale = 2, _Count = 3 };  // NOLINT

void SetImGuiContext(ImGuiContext* ctx);

void BeginFrame(ImDrawList* drawlist = nullptr);

void SetRect(float x, float y, eray::math::Vec2f size);

bool IsOverTransform();

bool IsUsingTransform();

bool Transform(eray::math::Transform3f& trans, const ::mini::Camera& camera, Mode mode, Operation operation,
               const std::function<void()>& on_use);

}  // namespace gizmo

}  // namespace mini

}  // namespace ImGui
