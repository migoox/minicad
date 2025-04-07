#pragma once

#include <functional>
#include <liberay/math/transform3_fwd.hpp>

namespace ImGui::mini {  // NOLINT

bool Transform(eray::math::Transform3f& trans, const std::function<void()>& on_use = []() {});

}
