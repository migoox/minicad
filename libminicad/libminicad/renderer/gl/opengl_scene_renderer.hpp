#pragma once

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/framebuffer.hpp>
#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/renderer/gl/opengl_scene_renderer.hpp>
#include <libminicad/renderer/gl/point_lists_renderer.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini::gl {

class OpenGLSceneRenderer;

struct SceneObjectRSCommandHandler {
  explicit SceneObjectRSCommandHandler(OpenGLSceneRenderer& _renderer, Scene& _scene,
                                       const SceneObjectRSCommand& _cmd_ctx)
      : renderer(_renderer), scene(_scene), cmd_ctx(_cmd_ctx) {}

  void operator()(const SceneObjectRSCommand::Internal::AddObject&);
  void operator()(const SceneObjectRSCommand::UpdateObjectMembers&);
  void operator()(const SceneObjectRSCommand::UpdateObjectVisibility&);
  void operator()(const SceneObjectRSCommand::Internal::DeleteObject&);

  // NOLINTBEGIN
  OpenGLSceneRenderer& renderer;
  Scene& scene;
  const SceneObjectRSCommand& cmd_ctx;
  // NOLINTEND
};

class OpenGLSceneRenderer final : public ISceneRenderer {
 public:
  OpenGLSceneRenderer()        = delete;
  ~OpenGLSceneRenderer() final = default;

  enum class SceneRendererCreationError : uint8_t {
    AssetsLoadingFailed         = 0,
    ShaderProgramCreationFailed = 1,
  };

  static std::expected<std::unique_ptr<ISceneRenderer>, SceneRendererCreationError> create(
      const std::filesystem::path& assets_path, eray::math::Vec2i win_size);

  void push_object_rs_cmd(const SceneObjectRSCommand& cmd) final;
  std::optional<::mini::SceneObjectRS> object_rs(const SceneObjectHandle& handle) final;
  void set_object_rs(const SceneObjectHandle& handle, const ::mini::SceneObjectRS& state) final;

  void push_object_rs_cmd(const PointListObjectRSCommand& cmd) final;
  std::optional<::mini::PointListObjectRS> object_rs(const PointListObjectHandle& handle) final;
  void set_object_rs(const PointListObjectHandle& handle, const ::mini::PointListObjectRS& state) final;

  void add_billboard(zstring_view name, const eray::res::Image& img) final;
  ::mini::BillboardRS& billboard(zstring_view name) final;

  void show_grid(bool show_grid) final { global_rs_.show_grid = show_grid; }
  bool is_grid_shown() const final { return global_rs_.show_grid; }

  void resize_viewport(eray::math::Vec2i win_size) final;
  std::unordered_set<int> sample_mouse_pick_box(size_t x, size_t y, size_t width, size_t height) const final;

  void update(Scene& scene) final;
  void render(Camera& camera) final;

 private:
  friend SceneObjectRSCommandHandler;
  friend PointListObjectRSCommandHandler;

  struct Shaders {
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> param;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> grid;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> polyline;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> bezier;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> sprite;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> instanced_sprite;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> helper_points;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> screen_quad;
  } shaders_;

  struct GlobalRS {
    std::unordered_map<zstring_view, BillboardRS> billboards;

    bool show_grid;
  } global_rs_;

  struct SceneObjectsRS {
    eray::driver::gl::VertexArray points_vao;
    eray::driver::gl::VertexArrays torus_vao;
    eray::driver::gl::TextureHandle point_txt;
    eray::driver::gl::TextureHandle helper_point_txt;

    std::vector<SceneObjectRSCommand> cmds;

    std::vector<SceneObjectHandle> transferred_points_buff;
    std::unordered_map<SceneObjectHandle, std::size_t> transferred_point_ind;

    std::vector<SceneObjectHandle> transferred_torus_buff;
    std::unordered_map<SceneObjectHandle, std::size_t> transferred_torus_ind;

    std::unordered_map<SceneObjectHandle, SceneObjectRS> scene_objects_rs;
  } scene_objs_rs_;

  PointListsRenderer point_list_renderer_;

  std::unique_ptr<eray::driver::gl::ViewportFramebuffer> framebuffer_;

 private:
  explicit OpenGLSceneRenderer(Shaders&& shaders, GlobalRS&& global_rs, SceneObjectsRS&& objs_rs,
                               PointListsRenderer&& point_list_objs_rs,
                               std::unique_ptr<eray::driver::gl::ViewportFramebuffer>&& framebuffer);
};

}  // namespace mini::gl
