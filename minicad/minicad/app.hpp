#pragma once

#include <liberay/driver/gl/shader_program.hpp>
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
#include <memory>
#include <minicad/camera/camera.hpp>
#include <minicad/camera/orbiting_camera_operator.hpp>
#include <minicad/cursor/cursor.hpp>

namespace mini {

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
  struct Members {
    eray::driver::gl::VertexArray box_vao;
    eray::driver::gl::VertexArray patch_vao;

    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> shader_prog;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> param_sh_prog;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> grid_sh_prog;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> sprite_sh_prog;

    GLuint cursor_txt;

    minicad::OrbitingCameraOperator orbiting_camera_operator;

    std::unique_ptr<minicad::Cursor> cursor;

    std::unique_ptr<minicad::Camera> camera;
    std::unique_ptr<eray::math::Transform3f> camera_gimbal;

    bool grid_on;
    bool use_ortho;

    Scene scene;
    // torus
    eray::math::Vec2i tess_level;
    float rad_minor;
    float rad_major;
  };

  MiniCadApp(std::unique_ptr<eray::os::Window> window, Members&& m);

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
