#pragma once

#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/os/app.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/timer.hpp>

namespace mini {

class MiniCadApp final : public eray::os::Application {
 public:
  MiniCadApp() = delete;
  static MiniCadApp create(std::unique_ptr<eray::os::Window> window);
  ~MiniCadApp();

 protected:
  void render(Duration delta) final;
  void update(Duration delta) final;

 private:
  struct Members {
    eray::driver::gl::VertexArray vao;
    eray::driver::gl::RenderingShaderProgram shader_prog;
  };

  MiniCadApp(std::unique_ptr<eray::os::Window> window, Members&& m);

  bool on_mouse_pressed(const eray::os::MouseButtonPressedEvent& ev);
  bool on_mouse_released(const eray::os::MouseButtonReleasedEvent& ev);
  bool on_resize(const eray::os::WindowResizedEvent& ev);
  bool on_scrolled(const eray::os::MouseScrolledEvent& ev);

 private:
  static constexpr float kScrollingSens = 0.036F;
  static constexpr float kRotationSens  = 0.006F;
  static constexpr float kMoveSens      = 0.001F;

  Members m_;
};

}  // namespace mini
