#include <glad/gl.h>
#include <imgui/imgui.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/driver/glsl_shader.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/transform3_fwd.hpp>
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
#include <memory>
#include <minicad/app.hpp>
#include <minicad/camera/orbiting_camera_operator.hpp>

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

      // Left Face
      -0.5f * width, -0.5f * height, +0.5f * depth, -1.0f, 0.0f, 0.0f,  //
      -0.5f * width, -0.5f * height, -0.5f * depth, -1.0f, 0.0f, 0.0f,  //
      -0.5f * width, +0.5f * height, -0.5f * depth, -1.0f, 0.0f, 0.0f,  //
      -0.5f * width, +0.5f * height, +0.5f * depth, -1.0f, 0.0f, 0.0f,  //

      // Right Face
      +0.5f * width, -0.5f * height, -0.5f * depth, 1.0f, 0.0f, 0.0f,  //
      +0.5f * width, -0.5f * height, +0.5f * depth, 1.0f, 0.0f, 0.0f,  //
      +0.5f * width, +0.5f * height, +0.5f * depth, 1.0f, 0.0f, 0.0f,  //
      +0.5f * width, +0.5f * height, -0.5f * depth, 1.0f, 0.0f, 0.0f,  //

      // Bottom Face
      -0.5f * width, -0.5f * height, +0.5f * depth, 0.0f, -1.0f, 0.0f,  //
      +0.5f * width, -0.5f * height, +0.5f * depth, 0.0f, -1.0f, 0.0f,  //
      +0.5f * width, -0.5f * height, -0.5f * depth, 0.0f, -1.0f, 0.0f,  //
      -0.5f * width, -0.5f * height, -0.5f * depth, 0.0f, -1.0f, 0.0f,  //

      // Top Face
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
}

gl::VertexArray get_patch_vao(float width = 1.F, float height = 1.F) {
  float vertices[] = {
      0.5F * width,  0.5F * height,   //
      -0.5F * width, 0.5F * height,   //
      0.5F * width,  -0.5F * height,  //
      -0.5F * width, -0.5F * height,  //
  };

  unsigned int indices[] = {
      0, 1, 2, 3  //
  };

  auto vbo = gl::VertexBuffer::create({
      gl::VertexBuffer::Attribute(0, 2, false),
  });
  vbo.buffer_data(vertices, gl::DataUsage::StaticDraw);

  auto ebo = gl::ElementBuffer::create();
  ebo.buffer_data(indices, gl::DataUsage::StaticDraw);

  return gl::VertexArray::create(std::move(vbo), std::move(ebo));
}

}  // namespace

MiniCadApp MiniCadApp::create(std::unique_ptr<Window> window) {
  glPatchParameteri(GL_PATCH_VERTICES, 4);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  auto manager = GLSLShaderManager();
  auto vert    = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "main.vert"));
  auto frag    = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "main.frag"));
  auto program = unwrap_or_panic(gl::RenderingShaderProgram::create("shader", std::move(vert), std::move(frag)));

  auto param_vert = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "param.vert"));
  auto param_frag = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "param.frag"));
  auto param_tesc = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "param.tesc"));
  auto param_tese = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "param.tese"));
  auto param_prog = unwrap_or_panic(gl::RenderingShaderProgram::create(
      "param_shader", std::move(param_vert), std::move(param_frag), std::move(param_tesc), std::move(param_tese)));

  auto camera = std::make_unique<minicad::Camera>(
      false, math::radians(90.F), static_cast<float>(window->size().x) / static_cast<float>(window->size().y), 0.1F,
      1000.F);
  camera->transform.set_local_pos(math::Vec3f(0.F, 0.F, -4.F));

  auto gimbal = std::make_unique<eray::math::Transform3f>();
  camera->transform.set_parent(*gimbal);

  return MiniCadApp(std::move(window),
                    {
                        .box_vao       = get_box_vao(),          //
                        .patch_vao     = get_patch_vao(),        //
                        .shader_prog   = std::move(program),     //
                        .param_sh_prog = std::move(param_prog),  //
                        .camera        = std::move(camera),      //
                        .camera_gimbal = std::move(gimbal),      //
                        .tess_level    = math::Vec2i(16, 16),    //
                        .rad_minor     = 2.F,                    //
                        .rad_major     = 4.F,                    //
                    });
}

MiniCadApp::MiniCadApp(std::unique_ptr<Window> window, Members&& m) : Application(std::move(window)), m_(std::move(m)) {
  window_->set_event_callback<WindowResizedEvent>(class_method_as_event_callback(this, &MiniCadApp::on_resize));
  window_->set_event_callback<MouseButtonPressedEvent>(
      class_method_as_event_callback(this, &MiniCadApp::on_mouse_pressed));
  window_->set_event_callback<MouseButtonReleasedEvent>(
      class_method_as_event_callback(this, &MiniCadApp::on_mouse_released));
  window_->set_event_callback<MouseScrolledEvent>(class_method_as_event_callback(this, &MiniCadApp::on_scrolled));
  window_->set_event_callback<KeyPressedEvent>(class_method_as_event_callback(this, &MiniCadApp::on_key_pressed));
}

void MiniCadApp::render(Application::Duration /* delta */) {
  ImGui::SetNextWindowSizeConstraints(ImVec2(300, 160), ImVec2(300, 160));
  ImGui::Begin("Torus");
  ImGui::Text("FPS: %d", fps_);
  ImGui::SliderInt("Tess Level X", &m_.tess_level.x, kMinTessLevel, kMaxTessLevel);
  ImGui::SliderInt("Tess Level Y", &m_.tess_level.y, kMinTessLevel, kMaxTessLevel);
  ImGui::SliderFloat("Rad Minor", &m_.rad_minor, 0.1F, 5.F);
  ImGui::SliderFloat("Rad Major", &m_.rad_major, 0.1F, 5.F);
  ImGui::End();

  glClearColor(0.5F, 0.6F, 0.6F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
  m_.param_sh_prog->set_uniform("mMat", math::Mat4f::identity());
  m_.param_sh_prog->set_uniform("vMat", m_.camera->view_matrix());
  m_.param_sh_prog->set_uniform("pMat", m_.camera->proj_matrix());
  m_.param_sh_prog->set_uniform("tessLevelX", m_.tess_level.x);
  m_.param_sh_prog->set_uniform("tessLevelY", m_.tess_level.y);
  m_.param_sh_prog->set_uniform("minorRad", m_.rad_minor);
  m_.param_sh_prog->set_uniform("majorRad", m_.rad_major);
  m_.param_sh_prog->bind();
  m_.patch_vao.bind();
  glDrawElements(GL_PATCHES, static_cast<GLsizei>(m_.patch_vao.ebo().count()), GL_UNSIGNED_INT, nullptr);
}

void MiniCadApp::update(Duration delta) {
  auto deltaf = std::chrono::duration<float>(delta).count();
  m_.orbiting_camera_operator.update(*m_.camera, *m_.camera_gimbal, math::Vec2f(window_->mouse_pos()), deltaf);
}

bool MiniCadApp::on_mouse_pressed(const MouseButtonPressedEvent& ev) {
  if (ev.is_on_ui()) {
    return false;
  }

  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonLeft) {
    m_.orbiting_camera_operator.start_looking_around(math::Vec2f(ev.x(), ev.y()));
  }

  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonMiddle) {
    m_.orbiting_camera_operator.start_pan(math::Vec2f(ev.x(), ev.y()));
  }

  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonRight) {
    m_.orbiting_camera_operator.start_rot(math::Vec2f(ev.x(), ev.y()));
  }

  return true;
}

bool MiniCadApp::on_mouse_released(const MouseButtonReleasedEvent& ev) {
  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonLeft) {
    m_.orbiting_camera_operator.stop_looking_around(*m_.camera, *m_.camera_gimbal);
  }
  m_.orbiting_camera_operator.stop_rot();
  m_.orbiting_camera_operator.stop_pan();
  return true;
}

bool MiniCadApp::on_resize(const WindowResizedEvent& ev) {  // NOLINT
  glViewport(0, 0, ev.width(), ev.height());

  return true;
}

bool MiniCadApp::on_scrolled(const MouseScrolledEvent& ev) {
  m_.orbiting_camera_operator.zoom(*m_.camera, static_cast<float>(ev.y_offset()));
  return true;
}

bool MiniCadApp::on_key_pressed(const eray::os::KeyPressedEvent& ev) {
  if (ev.key_code() == KeyCode::C) {
    m_.camera_gimbal->set_local_pos(math::Vec3f::filled(0.F));
    return true;
  }
  return false;
}

MiniCadApp::~MiniCadApp() {}

}  // namespace mini
