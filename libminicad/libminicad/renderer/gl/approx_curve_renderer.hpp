#pragma once
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/ruleof.hpp>
#include <libminicad/renderer/gl/buffer.hpp>
#include <libminicad/renderer/gl/subrenderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/handles.hpp>

namespace mini::gl {

class ApproxCurvesRenderer;

struct ApproxCurveRSCommandHandler {
  explicit ApproxCurveRSCommandHandler(const ApproxCurveRSCommand& _cmd_ctx, ApproxCurvesRenderer& _renderer,
                                       Scene& _scene)
      : cmd_ctx(_cmd_ctx), renderer(_renderer), scene(_scene) {}

  std::generator<eray::math::Vec3f> lines_generator(ref<ApproxCurve> curve);

  void operator()(const ApproxCurveRSCommand::Internal::AddObject&);
  void operator()(const ApproxCurveRSCommand::Internal::DeleteObject&);

  // NOLINTBEGIN
  const ApproxCurveRSCommand& cmd_ctx;
  ApproxCurvesRenderer& renderer;
  Scene& scene;
  // NOLINTEND
};

class ApproxCurvesRenderer : public SubRenderer<ApproxCurvesRenderer, ApproxCurveHandle, ApproxCurveRS,
                                                ApproxCurveRSCommand, ApproxCurveRSCommandHandler> {
 public:
  ApproxCurvesRenderer() = delete;

  using PointsChunksBuffer =
      ChunksBuffer<ApproxCurveHandle, eray::math::Vec3f, float, 3, [](const eray::math::Vec3f& vec, float* target) {
        target[0] = vec.x;
        target[1] = vec.y;
        target[2] = vec.z;
      }>;

  static ApproxCurvesRenderer create();

  void update_impl(Scene& scene);

  void render_curves() const;

 private:
  friend ApproxCurveRSCommandHandler;

  struct Members {
    eray::driver::gl::SimpleVertexArray curves_vao;
    PointsChunksBuffer curves;
  } m_;

  explicit ApproxCurvesRenderer(Members&& m);
};

}  // namespace mini::gl
