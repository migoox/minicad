#pragma once

#include <liberay/driver/gl/vertex_array.hpp>
#include <libminicad/renderer/gl/subrenderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/scene/handles.hpp>

namespace mini::gl {

class PointObjectRenderer;

struct PointObjectRSCommandHandler {
  explicit PointObjectRSCommandHandler(const PointObjectRSCommand& _cmd_ctx, PointObjectRenderer& _rs, Scene& _scene)
      : cmd_ctx(_cmd_ctx), renderer(_rs), scene(_scene) {}

  void operator()(const PointObjectRSCommand::Internal::AddObject&);
  void operator()(const PointObjectRSCommand::UpdateObjectMembers&);
  void operator()(const PointObjectRSCommand::UpdateObjectVisibility&);
  void operator()(const PointObjectRSCommand::Internal::DeleteObject&);

  // NOLINTBEGIN
  const PointObjectRSCommand& cmd_ctx;
  PointObjectRenderer& renderer;
  Scene& scene;
  // NOLINTEND
};

class PointObjectRenderer : public SubRenderer<PointObjectRenderer, PointObjectHandle, PointObjectRS,
                                               PointObjectRSCommand, PointObjectRSCommandHandler> {
 public:
  static PointObjectRenderer create();

  void render_control_points() const;

  void update_impl(Scene& scene);

 private:
  friend PointObjectRSCommandHandler;

  struct Members {
    eray::driver::gl::VertexArray points_vao;
    std::vector<PointObjectHandle> transferred_points_buff;
    std::unordered_map<PointObjectHandle, std::size_t> transferred_point_ind;
  } m_;

  explicit PointObjectRenderer(Members&& members);
};

}  // namespace mini::gl
