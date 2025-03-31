#pragma once

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/framebuffer.hpp>
#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/transform3.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/os/app.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/timer.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <memory>
#include <minicad/camera/camera.hpp>
#include <minicad/camera/orbiting_camera_operator.hpp>
#include <minicad/cursor/cursor.hpp>
#include <unordered_map>

namespace mini {

// TODO(migoox): dedicated opengl scene renderer in libminicad
struct RenderingState {
  eray::driver::gl::VertexArray points_vao;
  eray::driver::gl::VertexArray box_vao;

  eray::driver::gl::VertexArrays torus_vao;

  std::unordered_map<SceneObjectHandle, std::size_t> transferred_point_ind;
  std::vector<SceneObjectHandle> transferred_points_buff;

  std::unordered_map<SceneObjectHandle, std::size_t> transferred_torus_ind;
  std::vector<SceneObjectHandle> transferred_torus_buff;

  std::unordered_map<PointListObjectHandle, std::pair<GLuint, eray::driver::gl::ElementBuffer>> point_list_vaos;

  GLuint cursor_txt;
  GLuint point_txt;

  std::unique_ptr<eray::driver::gl::RenderingShaderProgram> param_sh_prog;
  std::unique_ptr<eray::driver::gl::RenderingShaderProgram> grid_sh_prog;
  std::unique_ptr<eray::driver::gl::RenderingShaderProgram> polyline_sh_prog;
  std::unique_ptr<eray::driver::gl::RenderingShaderProgram> sprite_sh_prog;
  std::unique_ptr<eray::driver::gl::RenderingShaderProgram> screen_quad_sh_prog;
  std::unique_ptr<eray::driver::gl::RenderingShaderProgram> instanced_sprite_sh_prog;

  std::unique_ptr<eray::driver::gl::ViewportFramebuffer> viewport_fb;
};

class MiniCadApp final : public eray::os::Application {
 public:
  MiniCadApp() = delete;
  static MiniCadApp create(std::unique_ptr<eray::os::Window> window);
  ~MiniCadApp();

 protected:
  void render(Duration delta) final;
  void render_gui(Duration delta) final;
  void update(Duration delta) final;

 private:
  enum class ToolState : uint8_t {
    Select = 0,
    Cursor = 1,
  };

  struct Members {
    minicad::OrbitingCameraOperator orbiting_camera_operator;

    std::unique_ptr<minicad::Cursor> cursor;

    std::unique_ptr<minicad::Camera> camera;
    std::unique_ptr<eray::math::Transform3f> camera_gimbal;

    bool grid_on;
    bool use_ortho;

    Scene scene;

    // TODO(migoox): state machine
    ToolState tool_state;

    // torus
    eray::math::Vec2i tess_level;
    float rad_minor;
    float rad_major;

    RenderingState rs;
  };

  MiniCadApp(std::unique_ptr<eray::os::Window> window, Members&& m);

  bool on_scene_object_added(SceneObjectVariant variant);
  bool on_scene_object_deleted(const SceneObjectHandle& handle);

  bool on_point_list_object_added();
  bool on_point_list_object_deleted(const PointListObjectHandle& handle);

  bool on_cursor_state_set();
  bool on_select_state_set();
  bool on_tool_action_start();

  bool on_mouse_pressed(const eray::os::MouseButtonPressedEvent& ev);
  bool on_mouse_released(const eray::os::MouseButtonReleasedEvent& ev);
  bool on_resize(const eray::os::WindowResizedEvent& ev);
  bool on_scrolled(const eray::os::MouseScrolledEvent& ev);
  bool on_key_pressed(const eray::os::KeyPressedEvent& ev);

 private:
  static constexpr int kMinTessLevel = 3;
  static constexpr int kMaxTessLevel = 64;

  Members m_;
};

}  // namespace mini
