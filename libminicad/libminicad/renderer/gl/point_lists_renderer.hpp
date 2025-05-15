#pragma once
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/ruleof.hpp>
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
  void operator()(const PointListObjectRSCommand::Internal::DeleteObject&) {}  // handled during flush
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

using PointsChunksBuffer =
    ChunksBuffer<PointListObjectHandle, eray::math::Vec3f, float, 3, [](const eray::math::Vec3f& vec, float* target) {
      target[0] = vec.x;
      target[1] = vec.y;
      target[2] = vec.z;
    }>;

struct PointListsRenderer {
  PointListsRenderer() = delete;

  static PointListsRenderer create();

  void push_cmd(const PointListObjectRSCommand& cmd);
  void update(Scene& scene);

  std::optional<PointListObjectRS> object_rs(const PointListObjectHandle& handle);
  void set_object_rs(const PointListObjectHandle& handle, const ::mini::PointListObjectRS& state);

  void render_polylines() const;
  void render_curves() const;
  void render_helper_points() const;

 private:
  friend PointListObjectRSCommandHandler;

  struct Members {
    eray::driver::gl::VertexArray helper_points_vao;
    PointsChunksBuffer helper_points;

    eray::driver::gl::VertexArray polylines_vao;
    PointsChunksBuffer polylines;

    eray::driver::gl::VertexArray curves_vao;
    PointsChunksBuffer curves;

    std::unordered_map<PointListObjectHandle, ::mini::PointListObjectRS> point_lists;
    std::vector<PointListObjectRSCommand> cmds;
  } m_;

  explicit PointListsRenderer(Members&& m);
};

}  // namespace mini::gl
