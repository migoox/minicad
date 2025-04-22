#pragma once

#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini {

struct SceneObjectRSCommand {
  struct AddObject {};

  struct UpdateObjectParams {};

  struct UpdateObjectVisibility {
    explicit UpdateObjectVisibility(VisibilityState vs) : new_visibility_state(vs) {}

    VisibilityState new_visibility_state;
  };

  struct DeleteObject {};

  using CommandVariant = std::variant<DeleteObject, UpdateObjectParams, UpdateObjectVisibility, AddObject>;

  explicit SceneObjectRSCommand(SceneObjectHandle _handle, CommandVariant _cmd) : handle(_handle), cmd(_cmd) {}
  SceneObjectRSCommand() = delete;

  SceneObjectHandle handle;
  CommandVariant cmd;
};

struct PointListObjectRSCommand {
  struct AddObject {};

  struct UpdateObjectParams {};

  struct UpdateObjectVisibility {
    explicit UpdateObjectVisibility(VisibilityState vs) : new_visibility_state(vs) {}

    VisibilityState new_visibility_state;
  };

  struct DeleteObject {};

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

  using CommandVariant = std::variant<DeleteObject, UpdateObjectParams, UpdateObjectVisibility, AddObject, ShowPolyline,
                                      ShowBernsteinControlPoints>;

  explicit PointListObjectRSCommand(PointListObjectHandle _handle, CommandVariant _cmd) : handle(_handle), cmd(_cmd) {}
  PointListObjectRSCommand() = delete;

  PointListObjectHandle handle;
  CommandVariant cmd;
};

}  // namespace mini
