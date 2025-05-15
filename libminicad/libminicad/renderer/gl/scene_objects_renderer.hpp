#pragma once

#include <liberay/driver/gl/vertex_array.hpp>
#include <libminicad/renderer/gl/subrenderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/scene/scene_object_handle.hpp>

namespace mini::gl {

class SceneObjectsRenderer;

struct SceneObjectRSCommandHandler {
  explicit SceneObjectRSCommandHandler(const SceneObjectRSCommand& _cmd_ctx, SceneObjectsRenderer& _rs, Scene& _scene)
      : cmd_ctx(_cmd_ctx), renderer(_rs), scene(_scene) {}

  void operator()(const SceneObjectRSCommand::Internal::AddObject&);
  void operator()(const SceneObjectRSCommand::UpdateObjectMembers&);
  void operator()(const SceneObjectRSCommand::UpdateObjectVisibility&);
  void operator()(const SceneObjectRSCommand::Internal::DeleteObject&);

  // NOLINTBEGIN
  const SceneObjectRSCommand& cmd_ctx;
  SceneObjectsRenderer& renderer;
  Scene& scene;
  // NOLINTEND
};

class SceneObjectsRenderer : public SubRenderer<SceneObjectsRenderer, SceneObjectHandle, SceneObjectRS,
                                                SceneObjectRSCommand, SceneObjectRSCommandHandler> {
 public:
  static SceneObjectsRenderer create();

  void render_control_points() const;
  void render_parameterized_surfaces() const;
  void render_parameterized_surfaces_filled() const;

  void update_impl(Scene& scene);

 private:
  friend SceneObjectRSCommandHandler;

  struct Members {
    eray::driver::gl::VertexArray points_vao;
    eray::driver::gl::VertexArrays torus_vao;

    std::vector<SceneObjectHandle> transferred_points_buff;
    std::unordered_map<SceneObjectHandle, std::size_t> transferred_point_ind;

    std::vector<SceneObjectHandle> transferred_torus_buff;
    std::unordered_map<SceneObjectHandle, std::size_t> transferred_torus_ind;
  } m_;

  explicit SceneObjectsRenderer(Members&& members);
};

}  // namespace mini::gl
