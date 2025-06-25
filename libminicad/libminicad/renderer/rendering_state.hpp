#pragma once

#include <liberay/math/vec.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <variant>

namespace mini {

// ---------------------------------------------------------------------------------------------------------------------
// - Billboard ---------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
struct BillboardRS {
  eray::math::Vec3f position = eray::math::Vec3f::zeros();
  float scale                = 0.04F;
  bool show                  = true;
};

// ---------------------------------------------------------------------------------------------------------------------
// - PointObject -------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
struct PointObjectRS {
  explicit PointObjectRS(VisibilityState visibility_state = VisibilityState::Visible) : visibility(visibility_state) {}

  VisibilityState visibility;
};

// ---------------------------------------------------------------------------------------------------------------------
// - ParamPrimitive ----------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
struct ParamPrimitiveRS {
  explicit ParamPrimitiveRS(VisibilityState visibility_state = VisibilityState::Visible)
      : visibility(visibility_state) {}

  VisibilityState visibility;
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
// - ApproxCurveRS -----------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct ApproxCurveRS {};

// ---------------------------------------------------------------------------------------------------------------------
// - Generic ObjectRS --------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

using ObjectRS = std::variant<PointObjectRS, CurveRS, PatchSurfaceRS, FillInSurfaceRS, ApproxCurveRS, ParamPrimitiveRS>;

}  // namespace mini
