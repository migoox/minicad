#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <liberay/driver/gl/framebuffer.hpp>
#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/zstring_view.hpp>
#include <libminicad/camera/camera.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <unordered_map>

namespace mini {

struct Billboard {
  explicit Billboard(eray::driver::gl::TextureHandle&& _texture) : texture(std::move(_texture)) {}

  eray::math::Vec3f position;
  float scale = 0.04F;
  bool show   = true;
  eray::driver::gl::TextureHandle texture;
};

struct PointListRenderingState {
  static PointListRenderingState create(const eray::driver::gl::VertexBuffer& vert_buff);
  eray::driver::gl::VertexArrayHandle vao;
  eray::driver::gl::ElementBuffer ebo;
  eray::driver::gl::ElementBuffer thrd_degree_bezier_ebo;
  bool show_polyline{true};
};

enum class VisibilityState : uint8_t {
  Visible   = 0,
  Selected  = 1,
  Invisible = 2,
};

class SceneRenderer {
 public:
  SceneRenderer() = delete;

  enum class SceneRendererCreationError : uint8_t {
    AssetsLoadingFailed         = 0,
    ShaderProgramCreationFailed = 1,
  };

  static std::expected<SceneRenderer, SceneRendererCreationError> create(const std::filesystem::path& assets_path);

  void update_scene_object(const SceneObject& obj);
  void update_point_list_object(const PointListObject& obj);

  void add_scene_object(const SceneObject& obj);
  void add_point_list_object(const PointListObject& obj);

  void delete_scene_object(const SceneObject& obj, Scene& scene);
  void delete_point_list_object(const PointListObject& obj);

  void update_visibility_state(const SceneObjectHandle& handle, VisibilityState state);

  void render(Scene& scene, eray::driver::gl::ViewportFramebuffer& fb, Camera& camera);

  void add_billboard(zstring_view name, const eray::res::Image& img);
  Billboard& billboard(zstring_view name);

  void show_grid(bool show_grid) { rs_.show_grid = show_grid; }
  bool is_grid() const { return rs_.show_grid; }

  void show_polyline(const PointListObjectHandle& obj, bool show);
  bool is_polyline_shown(const PointListObjectHandle& obj);

 private:
  struct RenderingState {
    eray::driver::gl::VertexArray points_vao;
    eray::driver::gl::VertexArray box_vao;
    eray::driver::gl::VertexArrays torus_vao;

    std::unordered_map<SceneObjectHandle, std::size_t> transferred_point_ind;
    std::vector<SceneObjectHandle> transferred_points_buff;

    std::unordered_map<SceneObjectHandle, std::size_t> transferred_torus_ind;
    std::vector<SceneObjectHandle> transferred_torus_buff;

    std::unordered_map<PointListObjectHandle, PointListRenderingState> point_lists_;

    eray::driver::gl::TextureHandle point_txt;

    std::unordered_map<zstring_view, Billboard> billboards;

    std::unordered_map<SceneObjectHandle, VisibilityState> visibility_states;

    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> param_sh_prog;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> grid_sh_prog;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> polyline_sh_prog;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> bezier_sh_prog;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> sprite_sh_prog;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> instanced_sprite_sh_prog;

    bool show_grid;
  };

  explicit SceneRenderer(RenderingState&& rs);

 private:
  RenderingState rs_;
};

}  // namespace mini
