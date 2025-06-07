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

class FillInSurfaceRenderer;

struct FillInSurfaceRSCommandHandler {
  explicit FillInSurfaceRSCommandHandler(const FillInSurfaceRSCommand& _cmd_ctx, FillInSurfaceRenderer& _renderer,
                                         Scene& _scene)
      : cmd_ctx(_cmd_ctx), renderer(_renderer), scene(_scene) {}

  void operator()(const FillInSurfaceRSCommand::Internal::AddObject&);
  void operator()(const FillInSurfaceRSCommand::Internal::UpdateControlPoints&);
  void operator()(const FillInSurfaceRSCommand::Internal::DeleteObject&);
  void operator()(const FillInSurfaceRSCommand::UpdateObjectVisibility&);
  void operator()(const FillInSurfaceRSCommand::ShowPolyline&);

  // NOLINTBEGIN
  const FillInSurfaceRSCommand& cmd_ctx;
  FillInSurfaceRenderer& renderer;
  Scene& scene;
  // NOLINTEND

 protected:
  static std::generator<eray::math::Vec3f> rational_bezier_patch_generator(ref<FillInSurface> surface);
  static size_t rational_bezier_patch_count(ref<FillInSurface> surface);
};

class FillInSurfaceRenderer : public SubRenderer<FillInSurfaceRenderer, FillInSurfaceHandle, FillInSurfaceRS,
                                                 FillInSurfaceRSCommand, FillInSurfaceRSCommandHandler> {
 public:
  FillInSurfaceRenderer() = delete;

  using PointsChunksBuffer =
      ChunksBuffer<FillInSurfaceHandle, eray::math::Vec3f, float, 3, [](const eray::math::Vec3f& vec, float* target) {
        target[0] = vec.x;
        target[1] = vec.y;
        target[2] = vec.z;
      }>;

  using IndicesChunksBuffer = ChunksBuffer<FillInSurfaceHandle, uint32_t, uint32_t, 1,
                                           [](const uint32_t& val, uint32_t* target) { target[0] = val; }>;

  static FillInSurfaceRenderer create();

  void update_impl(Scene& scene);

  void render_control_grids() const;
  void render_fill_in_surfaces() const;

 private:
  friend FillInSurfaceRSCommandHandler;

  struct Members {
    eray::driver::gl::VertexArray surfaces_vao;
    PointsChunksBuffer surfaces;

    eray::driver::gl::VertexArray control_grids_vao;
    PointsChunksBuffer control_grids;
  } m_;

  explicit FillInSurfaceRenderer(Members&& m);
};

}  // namespace mini::gl
