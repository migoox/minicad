#pragma once

#include <liberay/driver/gl/vertex_array.hpp>
#include <libminicad/renderer/gl/subrenderer.hpp>
#include <libminicad/scene/handles.hpp>
#include <libminicad/scene/param_primitive.hpp>

namespace mini::gl {

class ParamPrimitiveRenderer;

struct ParamPrimitiveRSCommandHandler {
  explicit ParamPrimitiveRSCommandHandler(const ParamPrimitiveRSCommand& _cmd_ctx, ParamPrimitiveRenderer& _rs,
                                          Scene& _scene)
      : cmd_ctx(_cmd_ctx), renderer(_rs), scene(_scene) {}

  void operator()(const ParamPrimitiveRSCommand::Internal::AddObject&);
  void operator()(const ParamPrimitiveRSCommand::Internal::UpdateTrimmingTextures&);
  void operator()(const ParamPrimitiveRSCommand::UpdateObjectMembers&);
  void operator()(const ParamPrimitiveRSCommand::UpdateObjectVisibility&);
  void operator()(const ParamPrimitiveRSCommand::Internal::DeleteObject&);

  // NOLINTBEGIN
  const ParamPrimitiveRSCommand& cmd_ctx;
  ParamPrimitiveRenderer& renderer;
  Scene& scene;
  // NOLINTEND
};

class ParamPrimitiveRenderer : public SubRenderer<ParamPrimitiveRenderer, ParamPrimitiveHandle, ParamPrimitiveRS,
                                                  ParamPrimitiveRSCommand, ParamPrimitiveRSCommandHandler> {
 public:
  static ParamPrimitiveRenderer create();

  void render_parameterized_surfaces() const;
  void render_parameterized_surfaces_filled() const;

  void update_impl(Scene& scene);

 private:
  friend ParamPrimitiveRSCommandHandler;

  struct Members {
    eray::driver::gl::VertexArrays torus_vao;
    std::vector<ParamPrimitiveHandle> transferred_torus_buff;
    std::unordered_map<ParamPrimitiveHandle, std::size_t> transferred_torus_ind;
  } m_;

  explicit ParamPrimitiveRenderer(Members&& members);
};

}  // namespace mini::gl
