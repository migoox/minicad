#pragma once

#include <liberay/math/vec.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <variant>

namespace mini {

struct BillboardRS {
  eray::math::Vec3f position = eray::math::Vec3f::zeros();
  float scale                = 0.04F;
  bool show                  = true;
};

// ---------------------------------------------------------------------------------------------------------------------
// - SceneObject -------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct PointRS {};

struct ParameterizedSurfaceRS {};

using SceneObjectVariantRS = std::variant<PointRS, ParameterizedSurfaceRS>;

struct SceneObjectRS {
  explicit SceneObjectRS(SceneObjectVariantRS&& variant   = PointRS{},
                         VisibilityState visibility_state = VisibilityState::Visible)
      : visibility(visibility_state), variant(std::move(variant)) {}

  VisibilityState visibility;
  SceneObjectVariantRS variant;
};

// ---------------------------------------------------------------------------------------------------------------------
// - Curve -------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct CurveRS {
  explicit CurveRS(VisibilityState visibility_state = VisibilityState::Visible, bool _show_polyline = true)
      : visibility(visibility_state), show_polyline(_show_polyline) {}

  VisibilityState visibility;
  bool show_polyline{true};
};

// ---------------------------------------------------------------------------------------------------------------------
// - PatchSurface ------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct PatchSurfaceRS {
  explicit PatchSurfaceRS(VisibilityState visibility_state = VisibilityState::Visible, bool _show_polyline = true)
      : visibility(visibility_state), show_polyline(_show_polyline) {}

  VisibilityState visibility;
  bool show_polyline{true};
};

// ---------------------------------------------------------------------------------------------------------------------
// - FillInSurface -----------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct FillInSurfaceRS {
  explicit FillInSurfaceRS(VisibilityState visibility_state = VisibilityState::Visible, bool _show_polyline = true)
      : visibility(visibility_state), show_polyline(_show_polyline) {}

  VisibilityState visibility;
  bool show_polyline{true};
};

// ---------------------------------------------------------------------------------------------------------------------
// - IntersectionCurveRS -----------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct IntersectionCurveRS {};

// ---------------------------------------------------------------------------------------------------------------------
// - Generic ObjectRS --------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

using ObjectRS = std::variant<SceneObjectRS, CurveRS, PatchSurfaceRS, FillInSurfaceRS, IntersectionCurveRS>;

}  // namespace mini
