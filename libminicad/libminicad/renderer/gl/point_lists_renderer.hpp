#pragma once
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/ruleof.hpp>
#include <libminicad/renderer/gl/buffer.hpp>
#include <libminicad/renderer/gl/subrenderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>

namespace mini::gl {

class PointListsRenderer;

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
  void operator()(const PointListObjectRSCommand::UpdateHelperPoints&);

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

class PointListsRenderer : public SubRenderer<PointListObjectHandle, PointListObjectRS, PointListObjectRSCommand> {
 public:
  PointListsRenderer() = delete;

  static PointListsRenderer create();

  void update(Scene& scene);

  std::optional<std::pair<PointListObjectHandle, size_t>> find_helper_point_by_idx(size_t idx) const;

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
  } m_;

  explicit PointListsRenderer(Members&& m);
};

}  // namespace mini::gl
