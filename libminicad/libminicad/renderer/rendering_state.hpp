#pragma once

#include <liberay/math/vec.hpp>
#include <libminicad/renderer/visibility_state.hpp>

namespace mini {

struct BillboardRS {
  eray::math::Vec3f position = eray::math::Vec3f::zeros();
  float scale                = 0.04F;
  bool show                  = true;
};

struct PointListObjectRS {
  explicit PointListObjectRS(VisibilityState visibility_state) : visibility(visibility_state) {}

  VisibilityState visibility;
  bool show_polyline{true};
};

struct SceneObjectRS {
  explicit SceneObjectRS(VisibilityState visibility_state) : visibility(visibility_state) {}

  VisibilityState visibility;
};

}  // namespace mini
