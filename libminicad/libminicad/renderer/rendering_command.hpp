#pragma once

#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/handles.hpp>

namespace mini {

// ---------------------------------------------------------------------------------------------------------------------
// - Command generic priorities ----------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct ImmediatePriority {
  static constexpr int kValue = 400;
};
struct HighPriority {
  static constexpr int kValue = 300;
};
struct MediumPriority {
  static constexpr int kValue = 200;
};
struct LowPriority {
  static constexpr int kValue = 100;
};
struct DeferredPriority {
  static constexpr int kValue = 0;
};

// Default priority
template <typename T>
struct RSCommandPriority {
  static constexpr int kValue = LowPriority::kValue;
};

constexpr auto kRSCommandPriorityComparer = [](auto&& arg_x, auto&& arg_y) {
  return RSCommandPriority<std::decay_t<decltype(arg_x)>>::kValue >
         RSCommandPriority<std::decay_t<decltype(arg_y)>>::kValue;
};

// ---------------------------------------------------------------------------------------------------------------------
// - SceneObjectRSCommand ----------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct PointObjectRSCommand {
  struct Internal {
    struct AddObject {};
    struct DeleteObject {};
  };

  struct UpdateObjectVisibility {
    explicit UpdateObjectVisibility(VisibilityState vs) : new_visibility_state(vs) {}

    VisibilityState new_visibility_state;
  };

  struct UpdateObjectMembers {};

  using CommandVariant =
      std::variant<Internal::DeleteObject, Internal::AddObject, UpdateObjectMembers, UpdateObjectVisibility>;

  explicit PointObjectRSCommand(PointObjectHandle _handle, CommandVariant _cmd) : handle(_handle), variant(_cmd) {}
  PointObjectRSCommand() = delete;

  PointObjectHandle handle;
  CommandVariant variant;
};

template <>
struct RSCommandPriority<PointObjectRSCommand::Internal::AddObject> {
  static constexpr int kValue = ImmediatePriority::kValue;
};

template <>
struct RSCommandPriority<PointObjectRSCommand::Internal::DeleteObject> {
  static constexpr int kValue = DeferredPriority::kValue;
};

// ---------------------------------------------------------------------------------------------------------------------
// - ParamPrimitiveRSCommand -------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct ParamPrimitiveRSCommand {
  struct Internal {
    struct AddObject {};
    struct DeleteObject {};
    struct UpdateTrimmingTextures {};
  };

  struct UpdateObjectVisibility {
    explicit UpdateObjectVisibility(VisibilityState vs) : new_visibility_state(vs) {}

    VisibilityState new_visibility_state;
  };

  struct UpdateObjectMembers {};

  using CommandVariant =
      std::variant<Internal::DeleteObject, Internal::AddObject, UpdateObjectMembers, UpdateObjectVisibility>;

  explicit ParamPrimitiveRSCommand(ParamPrimitiveHandle _handle, CommandVariant _cmd)
      : handle(_handle), variant(_cmd) {}
  ParamPrimitiveRSCommand() = delete;

  ParamPrimitiveHandle handle;
  CommandVariant variant;
};

template <>
struct RSCommandPriority<ParamPrimitiveRSCommand::Internal::AddObject> {
  static constexpr int kValue = ImmediatePriority::kValue;
};

template <>
struct RSCommandPriority<ParamPrimitiveRSCommand::Internal::DeleteObject> {
  static constexpr int kValue = DeferredPriority::kValue;
};

// ---------------------------------------------------------------------------------------------------------------------
// - CurveRSCommand ----------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct CurveRSCommand {
  struct Internal {
    struct AddObject {};
    struct DeleteObject {};
    struct UpdateControlPoints {};
  };

  struct UpdateObjectVisibility {
    explicit UpdateObjectVisibility(VisibilityState vs) : new_visibility_state(vs) {}

    VisibilityState new_visibility_state;
  };

  /**
   * @brief Applies to any point list, hides/shows the polyline between the scene points.
   *
   */
  struct ShowPolyline {
    explicit ShowPolyline(bool _show) : show(_show) {}
    bool show;
  };

  /**
   * @brief If the point list is a BSpline, this command will show/hide the bernstein control points.
   *
   */
  struct ShowBernsteinControlPoints {
    bool show;
  };

  struct UpdateHelperPoints {};

  using CommandVariant =
      std::variant<Internal::DeleteObject, Internal::AddObject, Internal::UpdateControlPoints, UpdateObjectVisibility,
                   ShowPolyline, ShowBernsteinControlPoints, UpdateHelperPoints>;

  explicit CurveRSCommand(CurveHandle _handle, CommandVariant _cmd) : handle(_handle), variant(_cmd) {}
  CurveRSCommand() = delete;

  CurveHandle handle;
  CommandVariant variant;
};

template <>
struct RSCommandPriority<CurveRSCommand::Internal::AddObject> {
  static constexpr int kValue = ImmediatePriority::kValue;
};

template <>
struct RSCommandPriority<CurveRSCommand::Internal::UpdateControlPoints> {
  static constexpr int kValue = HighPriority::kValue;
};

template <>
struct RSCommandPriority<CurveRSCommand::UpdateHelperPoints> {
  static constexpr int kValue = MediumPriority::kValue;
};

template <>
struct RSCommandPriority<CurveRSCommand::Internal::DeleteObject> {
  static constexpr int kValue = DeferredPriority::kValue;
};

// ---------------------------------------------------------------------------------------------------------------------
// - PatchSurfaceRSCommand ---------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct PatchSurfaceRSCommand {
  struct Internal {
    struct AddObject {};
    struct DeleteObject {};
    struct UpdateControlPoints {};
    struct UpdateTrimmingTextures {};
  };

  struct UpdateObjectVisibility {
    explicit UpdateObjectVisibility(VisibilityState vs) : new_visibility_state(vs) {}

    VisibilityState new_visibility_state;
  };

  /**
   * @brief Applies to any point list, hides/shows the polyline between the scene points.
   *
   */
  struct ShowPolyline {
    explicit ShowPolyline(bool _show) : show(_show) {}
    bool show;
  };

  using CommandVariant = std::variant<Internal::DeleteObject, Internal::AddObject, Internal::UpdateControlPoints,
                                      Internal::UpdateTrimmingTextures, UpdateObjectVisibility, ShowPolyline>;

  explicit PatchSurfaceRSCommand(PatchSurfaceHandle _handle, CommandVariant _cmd) : handle(_handle), variant(_cmd) {}
  PatchSurfaceRSCommand() = delete;

  PatchSurfaceHandle handle;
  CommandVariant variant;
};

template <>
struct RSCommandPriority<PatchSurfaceRSCommand::Internal::AddObject> {
  static constexpr int kValue = ImmediatePriority::kValue;
};

template <>
struct RSCommandPriority<PatchSurfaceRSCommand::Internal::UpdateControlPoints> {
  static constexpr int kValue = HighPriority::kValue;
};

template <>
struct RSCommandPriority<PatchSurfaceRSCommand::Internal::DeleteObject> {
  static constexpr int kValue = DeferredPriority::kValue;
};

// ---------------------------------------------------------------------------------------------------------------------
// - FillInSurfaceRSCommand --------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct FillInSurfaceRSCommand {
  struct Internal {
    struct AddObject {};
    struct DeleteObject {};
    struct UpdateControlPoints {};
  };

  struct UpdateObjectVisibility {
    explicit UpdateObjectVisibility(VisibilityState vs) : new_visibility_state(vs) {}

    VisibilityState new_visibility_state;
  };

  /**
   * @brief Applies to any point list, hides/shows the polyline between the scene points.
   *
   */
  struct ShowPolyline {
    explicit ShowPolyline(bool _show) : show(_show) {}
    bool show;
  };

  using CommandVariant = std::variant<Internal::DeleteObject, Internal::AddObject, Internal::UpdateControlPoints,
                                      UpdateObjectVisibility, ShowPolyline>;

  explicit FillInSurfaceRSCommand(FillInSurfaceHandle _handle, CommandVariant _cmd) : handle(_handle), variant(_cmd) {}
  FillInSurfaceRSCommand() = delete;

  FillInSurfaceHandle handle;
  CommandVariant variant;
};

template <>
struct RSCommandPriority<FillInSurfaceRSCommand::Internal::AddObject> {
  static constexpr int kValue = ImmediatePriority::kValue;
};

template <>
struct RSCommandPriority<FillInSurfaceRSCommand::Internal::UpdateControlPoints> {
  static constexpr int kValue = HighPriority::kValue;
};

template <>
struct RSCommandPriority<FillInSurfaceRSCommand::Internal::DeleteObject> {
  static constexpr int kValue = DeferredPriority::kValue;
};

// ---------------------------------------------------------------------------------------------------------------------
// - ApproxCurveRSCommand ----------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct ApproxCurveRSCommand {
  struct Internal {
    struct AddObject {};
    struct DeleteObject {};
  };

  using CommandVariant = std::variant<Internal::DeleteObject, Internal::AddObject>;

  explicit ApproxCurveRSCommand(ApproxCurveHandle _handle, CommandVariant _cmd) : handle(_handle), variant(_cmd) {}
  ApproxCurveRSCommand() = delete;

  ApproxCurveHandle handle;
  CommandVariant variant;
};

template <>
struct RSCommandPriority<ApproxCurveRSCommand::Internal::AddObject> {
  static constexpr int kValue = ImmediatePriority::kValue;
};

template <>
struct RSCommandPriority<ApproxCurveRSCommand::Internal::DeleteObject> {
  static constexpr int kValue = DeferredPriority::kValue;
};

// ---------------------------------------------------------------------------------------------------------------------
// - Generic RSCommand -------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

using RSCommand = std::variant<PointObjectRSCommand, CurveRSCommand, PatchSurfaceRSCommand, FillInSurfaceRSCommand,
                               ApproxCurveRSCommand, ParamPrimitiveRSCommand>;

}  // namespace mini
