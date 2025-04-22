#pragma once

#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini {

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

  explicit SceneObjectRSCommand(SceneObjectHandle _handle, CommandVariant _cmd) : handle(_handle), cmd(_cmd) {}
  SceneObjectRSCommand() = delete;

  SceneObjectHandle handle;
  CommandVariant cmd;
};

struct PointListObjectRSCommand {
  struct Internal {
    struct AddObject {};
    struct DeleteObject {};
  };

  struct UpdateObjectMembers {};

  struct UpdateObjectVisibility {
    explicit UpdateObjectVisibility(VisibilityState vs) : new_visibility_state(vs) {}

    VisibilityState new_visibility_state;
  };

  /**
   * @brief Applies to any point list, hides/shows the polyline between the scene points.
   *
   */
  struct ShowPolyline {
    bool show;
  };

  /**
   * @brief If the point list is a BSpline, this command will show/hide the bernstein control points.
   *
   */
  struct ShowBernsteinControlPoints {
    bool show;
  };

  /**
   * @brief If the point list is a BSpline, this command will update the bernstein control points.
   *
   */
  struct UpdateBernsteinControlPoints {
    bool show;
  };

  using CommandVariant =
      std::variant<Internal::DeleteObject, Internal::AddObject, UpdateObjectMembers, UpdateObjectVisibility,
                   ShowPolyline, ShowBernsteinControlPoints, UpdateBernsteinControlPoints>;

  explicit PointListObjectRSCommand(PointListObjectHandle _handle, CommandVariant _cmd) : handle(_handle), cmd(_cmd) {}
  PointListObjectRSCommand() = delete;

  PointListObjectHandle handle;
  CommandVariant cmd;
};

}  // namespace mini
