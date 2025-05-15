#pragma once

#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini {

//-- Command generic priorities ----------------------------------------------------------------------------------------

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

//-- SceneObjectRSCommand ----------------------------------------------------------------------------------------------

struct SceneObjectRSCommand {
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

  explicit SceneObjectRSCommand(SceneObjectHandle _handle, CommandVariant _cmd) : handle(_handle), variant(_cmd) {}
  SceneObjectRSCommand() = delete;

  SceneObjectHandle handle;
  CommandVariant variant;
};

template <>
struct RSCommandPriority<SceneObjectRSCommand::Internal::AddObject> {
  static constexpr int kValue = ImmediatePriority::kValue;
};

template <>
struct RSCommandPriority<SceneObjectRSCommand::Internal::DeleteObject> {
  static constexpr int kValue = DeferredPriority::kValue;
};

//-- PointListObjectRSCommand ------------------------------------------------------------------------------------------

struct PointListObjectRSCommand {
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

  explicit PointListObjectRSCommand(PointListObjectHandle _handle, CommandVariant _cmd)
      : handle(_handle), variant(_cmd) {}
  PointListObjectRSCommand() = delete;

  PointListObjectHandle handle;
  CommandVariant variant;
};

template <>
struct RSCommandPriority<PointListObjectRSCommand::Internal::AddObject> {
  static constexpr int kValue = ImmediatePriority::kValue;
};

template <>
struct RSCommandPriority<PointListObjectRSCommand::Internal::UpdateControlPoints> {
  static constexpr int kValue = HighPriority::kValue;
};

template <>
struct RSCommandPriority<PointListObjectRSCommand::UpdateHelperPoints> {
  static constexpr int kValue = MediumPriority::kValue;
};

template <>
struct RSCommandPriority<PointListObjectRSCommand::Internal::DeleteObject> {
  static constexpr int kValue = DeferredPriority::kValue;
};

}  // namespace mini
