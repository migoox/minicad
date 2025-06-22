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

class IntersectionCurvesRenderer;

struct IntersectionCurveRSCommandHandler {
  explicit IntersectionCurveRSCommandHandler(const IntersectionCurveRSCommand& _cmd_ctx,
                                             IntersectionCurvesRenderer& _renderer, Scene& _scene)
      : cmd_ctx(_cmd_ctx), renderer(_renderer), scene(_scene) {}

  std::generator<eray::math::Vec3f> lines_generator(ref<IntersectionCurve> curve);

  void operator()(const IntersectionCurveRSCommand::Internal::AddObject&);
  void operator()(const IntersectionCurveRSCommand::Internal::DeleteObject&);

  // NOLINTBEGIN
  const IntersectionCurveRSCommand& cmd_ctx;
  IntersectionCurvesRenderer& renderer;
  Scene& scene;
  // NOLINTEND
};

class IntersectionCurvesRenderer
    : public SubRenderer<IntersectionCurvesRenderer, IntersectionCurveHandle, IntersectionCurveRS,
                         IntersectionCurveRSCommand, IntersectionCurveRSCommandHandler> {
 public:
  IntersectionCurvesRenderer() = delete;

  using PointsChunksBuffer = ChunksBuffer<IntersectionCurveHandle, eray::math::Vec3f, float, 3,
                                          [](const eray::math::Vec3f& vec, float* target) {
                                            target[0] = vec.x;
                                            target[1] = vec.y;
                                            target[2] = vec.z;
                                          }>;

  static IntersectionCurvesRenderer create();

  void update_impl(Scene& scene);

  void render_curves() const;

 private:
  friend IntersectionCurveRSCommandHandler;

  struct Members {
    eray::driver::gl::SimpleVertexArray curves_vao;
    PointsChunksBuffer curves;
  } m_;

  explicit IntersectionCurvesRenderer(Members&& m);
};

}  // namespace mini::gl
