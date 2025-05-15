#pragma once

#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini {

struct RSCommand {
  bool is_handled{};
};

struct SceneObjectRSCommand : public RSCommand {
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

struct PointListObjectRSCommand : public RSCommand {
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

}  // namespace mini
