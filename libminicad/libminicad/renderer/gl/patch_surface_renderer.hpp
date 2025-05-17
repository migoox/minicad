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

class PatchSurfaceRenderer;

struct PatchSurfaceRSCommandHandler {
  explicit PatchSurfaceRSCommandHandler(const PatchSurfaceRSCommand& _cmd_ctx, PatchSurfaceRenderer& _renderer,
                                        Scene& _scene)
      : cmd_ctx(_cmd_ctx), renderer(_renderer), scene(_scene) {}

  void operator()(const PatchSurfaceRSCommand::Internal::AddObject&);
  void operator()(const PatchSurfaceRSCommand::Internal::UpdateControlPoints&);
  void operator()(const PatchSurfaceRSCommand::Internal::DeleteObject&);
  void operator()(const PatchSurfaceRSCommand::UpdateObjectVisibility&);
  void operator()(const PatchSurfaceRSCommand::ShowPolyline&);

  // NOLINTBEGIN
  const PatchSurfaceRSCommand& cmd_ctx;
  PatchSurfaceRenderer& renderer;
  Scene& scene;
  // NOLINTEND
};

class PatchSurfaceRenderer : public SubRenderer<PatchSurfaceRenderer, PatchSurfaceHandle, PatchSurfaceRS,
                                                PatchSurfaceRSCommand, PatchSurfaceRSCommandHandler> {
 public:
  PatchSurfaceRenderer() = delete;

  using PointsChunksBuffer =
      ChunksBuffer<CurveHandle, eray::math::Vec3f, float, 3, [](const eray::math::Vec3f& vec, float* target) {
        target[0] = vec.x;
        target[1] = vec.y;
        target[2] = vec.z;
      }>;

  static PatchSurfaceRenderer create();

  void update_impl(Scene& scene);

  void render_control_grids() const;
  void render_surfaces() const;

 private:
  friend PatchSurfaceRSCommandHandler;

  struct Members {
    eray::driver::gl::VertexArray surfaces_vao;
    PointsChunksBuffer surfaces;

    eray::driver::gl::VertexArray control_grids_vao;
    PointsChunksBuffer control_grids;
  } m_;

  explicit PatchSurfaceRenderer(Members&& m);
};

}  // namespace mini::gl
