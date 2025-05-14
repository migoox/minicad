#pragma once
#include <liberay/driver/gl/vertex_array.hpp>
#include <libminicad/renderer/gl/buffer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini::gl {

struct PointListsRenderer;

struct PointListObjectRSCommandHandler {
  explicit PointListObjectRSCommandHandler(const PointListObjectRSCommand& _cmd_ctx, PointListsRenderer& _rs,
                                           PointListObject& _obj, PointListObjectRS& _obj_rs)
      : cmd_ctx(_cmd_ctx), renderer(_rs), obj(_obj), obj_rs(_obj_rs) {}

  void operator()(const PointListObjectRSCommand::Internal::AddObject&);
  void operator()(const PointListObjectRSCommand::Internal::UpdateControlPoints&);
  void operator()(const PointListObjectRSCommand::Internal::DeleteObject&) {}
  void operator()(const PointListObjectRSCommand::UpdateObjectVisibility&);
  void operator()(const PointListObjectRSCommand::ShowPolyline&);
  void operator()(const PointListObjectRSCommand::ShowBernsteinControlPoints&);
  void operator()(const PointListObjectRSCommand::UpdateBernsteinControlPoints&);

  // NOLINTBEGIN
  const PointListObjectRSCommand& cmd_ctx;
  PointListsRenderer& renderer;
  PointListObject& obj;
  PointListObjectRS& obj_rs;
  // NOLINTEND
};

struct PointListsRenderer {
  PointListsRenderer() = delete;

  static PointListsRenderer create();

  void push_cmd(const PointListObjectRSCommand& cmd);
  void update(Scene& scene);

  std::optional<PointListObjectRS> object_rs(const PointListObjectHandle& handle);
  void set_object_rs(const PointListObjectHandle& handle, const ::mini::PointListObjectRS& state);

  void render_polylines();
  void render_curves();
  void render_helper_points();

 private:
  friend PointListObjectRSCommandHandler;

  PointListsRenderer(PointsBuffer<PointListObjectHandle>&& helper_points,
                     PointsBuffer<PointListObjectHandle>&& polylines, PointsBuffer<PointListObjectHandle>&& curves);

  PointsBuffer<PointListObjectHandle> helper_points_;

  PointsBuffer<PointListObjectHandle> polylines_;
  std::vector<PointListObjectHandle> polylines_to_delete_;

  PointsBuffer<PointListObjectHandle> curves_;
  std::vector<PointListObjectHandle> curves_to_delete_;

  std::unordered_map<PointListObjectHandle, ::mini::PointListObjectRS> point_lists_;
  std::vector<PointListObjectRSCommand> cmds_;
};

}  // namespace mini::gl
