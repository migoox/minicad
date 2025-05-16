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

struct CurveRSCommandHandler {
  explicit CurveRSCommandHandler(const CurveRSCommand& _cmd_ctx, PointListsRenderer& _renderer, Scene& _scene)
      : cmd_ctx(_cmd_ctx), renderer(_renderer), scene(_scene) {}

  void operator()(const CurveRSCommand::Internal::AddObject&);
  void operator()(const CurveRSCommand::Internal::UpdateControlPoints&);
  void operator()(const CurveRSCommand::Internal::DeleteObject&);
  void operator()(const CurveRSCommand::UpdateObjectVisibility&);
  void operator()(const CurveRSCommand::ShowPolyline&);
  void operator()(const CurveRSCommand::ShowBernsteinControlPoints&);
  void operator()(const CurveRSCommand::UpdateHelperPoints&);

  // NOLINTBEGIN
  const CurveRSCommand& cmd_ctx;
  PointListsRenderer& renderer;
  Scene& scene;
  // NOLINTEND
};

using PointsChunksBuffer =
    ChunksBuffer<CurveHandle, eray::math::Vec3f, float, 3, [](const eray::math::Vec3f& vec, float* target) {
      target[0] = vec.x;
      target[1] = vec.y;
      target[2] = vec.z;
    }>;

class PointListsRenderer
    : public SubRenderer<PointListsRenderer, CurveHandle, CurveRS, CurveRSCommand, CurveRSCommandHandler> {
 public:
  PointListsRenderer() = delete;

  static PointListsRenderer create();

  void update_impl(Scene& scene);

  std::optional<std::pair<CurveHandle, size_t>> find_helper_point_by_idx(size_t idx) const;

  void render_polylines() const;
  void render_curves() const;
  void render_helper_points() const;

 private:
  friend CurveRSCommandHandler;

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
