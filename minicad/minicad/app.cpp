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

#include "liberay/driver/gl/buffer.hpp"
#include "liberay/driver/gl/gl_error.hpp"
#include "liberay/math/transform3_fwd.hpp"

namespace mini {

// NOLINTBEGIN
using namespace eray;
using namespace eray::driver;
using namespace eray::os;
using namespace eray::util;
// NOLINTEND

namespace {

gl::VertexArray get_box_vao(float width = 1.f, float height = 1.f, float depth = 1.f) {
  float vertices[] = {
      // Front Face
      -0.5f * width, -0.5f * height, -0.5f * depth, 0.0f, 0.0f, -1.0f,  //
      +0.5f * width, -0.5f * height, -0.5f * depth, 0.0f, 0.0f, -1.0f,  //
      +0.5f * width, +0.5f * height, -0.5f * depth, 0.0f, 0.0f, -1.0f,  //
      -0.5f * width, +0.5f * height, -0.5f * depth, 0.0f, 0.0f, -1.0f,  //

      // Back Face
      +0.5f * width, -0.5f * height, +0.5f * depth, 0.0f, 0.0f, 1.0f,  //
      -0.5f * width, -0.5f * height, +0.5f * depth, 0.0f, 0.0f, 1.0f,  //
      -0.5f * width, +0.5f * height, +0.5f * depth, 0.0f, 0.0f, 1.0f,  //
      +0.5f * width, +0.5f * height, +0.5f * depth, 0.0f, 0.0f, 1.0f,  //

      //    Left Face
      -0.5f * width, -0.5f * height, +0.5f * depth, -1.0f, 0.0f, 0.0f,  //
      -0.5f * width, -0.5f * height, -0.5f * depth, -1.0f, 0.0f, 0.0f,  //
      -0.5f * width, +0.5f * height, -0.5f * depth, -1.0f, 0.0f, 0.0f,  //
      -0.5f * width, +0.5f * height, +0.5f * depth, -1.0f, 0.0f, 0.0f,  //

      //    Right Face
      +0.5f * width, -0.5f * height, -0.5f * depth, 1.0f, 0.0f, 0.0f,  //
      +0.5f * width, -0.5f * height, +0.5f * depth, 1.0f, 0.0f, 0.0f,  //
      +0.5f * width, +0.5f * height, +0.5f * depth, 1.0f, 0.0f, 0.0f,  //
      +0.5f * width, +0.5f * height, -0.5f * depth, 1.0f, 0.0f, 0.0f,  //

      //    Bottom Face
      -0.5f * width, -0.5f * height, +0.5f * depth, 0.0f, -1.0f, 0.0f,  //
      +0.5f * width, -0.5f * height, +0.5f * depth, 0.0f, -1.0f, 0.0f,  //
      +0.5f * width, -0.5f * height, -0.5f * depth, 0.0f, -1.0f, 0.0f,  //
      -0.5f * width, -0.5f * height, -0.5f * depth, 0.0f, -1.0f, 0.0f,  //

      //    Top Face
      -0.5f * width, +0.5f * height, -0.5f * depth, 0.0f, 1.0f, 0.0f,  //
      +0.5f * width, +0.5f * height, -0.5f * depth, 0.0f, 1.0f, 0.0f,  //
      +0.5f * width, +0.5f * height, +0.5f * depth, 0.0f, 1.0f, 0.0f,  //
      -0.5f * width, +0.5f * height, +0.5f * depth, 0.0f, 1.0f, 0.0f   //

  };

  unsigned int indices[] = {
      0,  2,  1,  0,  3,  2,   //
      4,  6,  5,  4,  7,  6,   //
      8,  10, 9,  8,  11, 10,  //
      12, 14, 13, 12, 15, 14,  //
      16, 18, 17, 16, 19, 18,  //
      20, 22, 21, 20, 23, 22   //
  };

  auto vbo = gl::VertexBuffer::create({
      gl::VertexBuffer::Attribute(0, 3, false),  // positions
      gl::VertexBuffer::Attribute(1, 3, false),  // normals
  });
  vbo.buffer_data(vertices, gl::DataUsage::StaticDraw);

  auto ebo = gl::ElementBuffer::create();
  ebo.buffer_data(indices, gl::DataUsage::StaticDraw);

  return gl::VertexArray::create(std::move(vbo), std::move(ebo));
}  // namespace

}  // namespace

MiniCadApp MiniCadApp::create(std::unique_ptr<Window> window) {
  //   float vertices[] = {
  //       0.5F,  0.5F,  -1.0F,  //
  //       0.5F,  -0.5F, -1.0F,  //
  //       -0.5F, -0.5F, -1.0F,  //
  //       -0.5F, 0.5F,  -1.0F,  //
  //   };

  //   unsigned int indices[] = {
  //       0, 1, 2,  //
  //       2, 3, 0   //
  //   };

  //   auto vbo = gl::VertexBuffer::create({
  //       gl::VertexBuffer::Attribute(0, 3, false),
  //   });
  //   vbo.buffer_data(vertices, gl::DataUsage::StaticDraw);

  //   auto ebo = gl::ElementBuffer::create();
  //   ebo.buffer_data(indices, gl::DataUsage::StaticDraw);

  auto manager = GLSLShaderManager();
  auto vert    = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "main.vert"));
  auto frag    = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "main.frag"));
  auto program = unwrap_or_panic(gl::RenderingShaderProgram::create("shader", std::move(vert), std::move(frag)));

  auto camera = std::make_unique<minicad::Camera>(
      false, math::radians(45.F), static_cast<float>(window->size().x) / static_cast<float>(window->size().y), 0.1F,
      1000.F);

  return MiniCadApp(std::move(window),
                    {
                        .box_vao     = get_box_vao(),       //
                        .shader_prog = std::move(program),  //
                        .camera      = std::move(camera),
                    });
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

  glEnable(GL_DEPTH_TEST);
  // TODO(migoox): lazy loading
  m_.shader_prog->set_uniform("mMat", math::Mat4f::identity());
  m_.shader_prog->set_uniform("vMat", m_.camera->view_matrix());
  m_.shader_prog->set_uniform("pMat", m_.camera->proj_matrix());
  glClearColor(0.5F, 0.6F, 0.6F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  m_.shader_prog->bind();
  m_.box_vao.bind();
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_.box_vao.ebo().count()), GL_UNSIGNED_INT, nullptr);
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
