#include <glad/gl.h>
#include <imgui/imgui.h>

#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/driver/glsl_shader.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/os/app.hpp>
#include <liberay/os/system.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window/input_codes.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/timer.hpp>
#include <minicad/app.hpp>

namespace mini {

// NOLINTBEGIN
using namespace eray::driver;
using namespace eray::os;
using namespace eray::math;
using namespace eray::util;
// NOLINTEND

MiniCadApp MiniCadApp::create(std::unique_ptr<Window> window) {
  float vertices[] = {
      1.0F,  1.0F,   //
      1.0F,  -1.0F,  //
      -1.0F, -1.0F,  //
      -1.0F, 1.0F    //
  };

  unsigned int indices[] = {
      0, 1, 2,  //
      2, 3, 0   //
  };

  auto vbo = gl::VertexBuffer::create({
      gl::VertexBuffer::Attribute(0, 2, false),
  });
  vbo.buffer_data(vertices, gl::DataUsage::StaticDraw);

  auto ebo = gl::ElementBuffer::create();
  ebo.buffer_data(indices, gl::DataUsage::StaticDraw);

  auto manager = GLSLShaderManager();
  auto vert    = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "main.vert"));
  auto frag    = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "main.frag"));
  auto program = unwrap_or_panic(gl::RenderingShaderProgram::create("test", std::move(vert), std::move(frag)));

  return MiniCadApp(std::move(window),
                    {.vao = gl::VertexArray::create(std::move(vbo), std::move(ebo)), .shader_prog = program});
}

MiniCadApp::MiniCadApp(std::unique_ptr<Window> window, Members&& m) : Application(std::move(window)), m_(std::move(m)) {
  window_->set_event_callback<WindowResizedEvent>(class_method_as_event_callback(this, &MiniCadApp::on_resize));
  window_->set_event_callback<MouseButtonPressedEvent>(
      class_method_as_event_callback(this, &MiniCadApp::on_mouse_pressed));
  window_->set_event_callback<MouseButtonReleasedEvent>(
      class_method_as_event_callback(this, &MiniCadApp::on_mouse_released));
  window_->set_event_callback<MouseScrolledEvent>(class_method_as_event_callback(this, &MiniCadApp::on_scrolled));
}

void MiniCadApp::render(Application::Duration /* delta */) {
  ImGui::SetNextWindowSizeConstraints(ImVec2(280, 220), ImVec2(280, 220));
  ImGui::Begin("Scene");

  ImGui::Text("FPS: %d", fps_);
  ImGui::End();

  glClearColor(0.5F, 0.6F, 0.6F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
}

void MiniCadApp::update(Duration /* delta */) {}

bool MiniCadApp::on_mouse_pressed(const MouseButtonPressedEvent& ev) { return true; }

bool MiniCadApp::on_mouse_released(const MouseButtonReleasedEvent& ev) { return true; }

bool MiniCadApp::on_resize(const WindowResizedEvent& ev) {  // NOLINT
  glViewport(0, 0, ev.width(), ev.height());

  return true;
}

bool MiniCadApp::on_scrolled(const MouseScrolledEvent& ev) { return true; }

MiniCadApp::~MiniCadApp() {}

}  // namespace mini
