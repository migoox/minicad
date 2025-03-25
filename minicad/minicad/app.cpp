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

gl::VertexArray get_box_vao(float width = 1.F, float height = 1.F, float depth = 1.F) {
  float vertices[] = {
      // Front Face
      -0.5F * width, -0.5F * height, -0.5F * depth, 0.0F, 0.0F, -1.0F,  //
      +0.5F * width, -0.5F * height, -0.5F * depth, 0.0F, 0.0F, -1.0F,  //
      +0.5F * width, +0.5F * height, -0.5F * depth, 0.0F, 0.0F, -1.0F,  //
      -0.5F * width, +0.5F * height, -0.5F * depth, 0.0F, 0.0F, -1.0F,  //

      // Back Face
      +0.5F * width, -0.5F * height, +0.5F * depth, 0.0F, 0.0F, 1.0F,  //
      -0.5F * width, -0.5F * height, +0.5F * depth, 0.0F, 0.0F, 1.0F,  //
      -0.5F * width, +0.5F * height, +0.5F * depth, 0.0F, 0.0F, 1.0F,  //
      +0.5F * width, +0.5F * height, +0.5F * depth, 0.0F, 0.0F, 1.0F,  //

      // Left Face
      -0.5F * width, -0.5F * height, +0.5F * depth, -1.0F, 0.0F, 0.0F,  //
      -0.5F * width, -0.5F * height, -0.5F * depth, -1.0F, 0.0F, 0.0F,  //
      -0.5F * width, +0.5F * height, -0.5F * depth, -1.0F, 0.0F, 0.0F,  //
      -0.5F * width, +0.5F * height, +0.5F * depth, -1.0F, 0.0F, 0.0F,  //

      // Right Face
      +0.5F * width, -0.5F * height, -0.5F * depth, 1.0F, 0.0F, 0.0F,  //
      +0.5F * width, -0.5F * height, +0.5F * depth, 1.0F, 0.0F, 0.0F,  //
      +0.5F * width, +0.5F * height, +0.5F * depth, 1.0F, 0.0F, 0.0F,  //
      +0.5F * width, +0.5F * height, -0.5F * depth, 1.0F, 0.0F, 0.0F,  //

      // Bottom Face
      -0.5F * width, -0.5F * height, +0.5F * depth, 0.0F, -1.0F, 0.0F,  //
      +0.5F * width, -0.5F * height, +0.5F * depth, 0.0F, -1.0F, 0.0F,  //
      +0.5F * width, -0.5F * height, -0.5F * depth, 0.0F, -1.0F, 0.0F,  //
      -0.5F * width, -0.5F * height, -0.5F * depth, 0.0F, -1.0F, 0.0F,  //

      // Top Face
      -0.5F * width, +0.5F * height, -0.5F * depth, 0.0F, 1.0F, 0.0F,  //
      +0.5F * width, +0.5F * height, -0.5F * depth, 0.0F, 1.0F, 0.0F,  //
      +0.5F * width, +0.5F * height, +0.5F * depth, 0.0F, 1.0F, 0.0F,  //
      -0.5F * width, +0.5F * height, +0.5F * depth, 0.0F, 1.0F, 0.0F   //

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

GLuint get_texture(res::Image& image) {
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width(), image.height(), 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
               image.raw());
  glBindTexture(GL_TEXTURE_2D, 0);

  return texture;
}

}  // namespace

MiniCadApp MiniCadApp::create(std::unique_ptr<Window> window) {
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

  auto grid_vert = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "grid.vert"));
  auto grid_frag = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "grid.frag"));
  auto grid_prog =
      unwrap_or_panic(gl::RenderingShaderProgram::create("grid_shader", std::move(grid_vert), std::move(grid_frag)));

  auto sprite_vert = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "sprite.vert"));
  auto sprite_frag = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "sprite.frag"));
  auto sprite_prog = unwrap_or_panic(
      gl::RenderingShaderProgram::create("billboard_shader", std::move(sprite_vert), std::move(sprite_frag)));

  auto cursor_img = unwrap_or_panic(res::Image::load_from_path(System::executable_dir() / "assets" / "cursor.png"));

  auto camera = std::make_unique<minicad::Camera>(
      false, math::radians(90.F), static_cast<float>(window->size().x) / static_cast<float>(window->size().y), 0.1F,
      1000.F);
  camera->transform.set_local_pos(math::Vec3f(0.F, 0.F, 4.F));

  auto gimbal = std::make_unique<eray::math::Transform3f>();
  camera->transform.set_parent(*gimbal);

  return MiniCadApp(std::move(window),
                    {
                        .box_vao        = get_box_vao(),                        //
                        .patch_vao      = get_patch_vao(),                      //
                        .shader_prog    = std::move(program),                   //
                        .param_sh_prog  = std::move(param_prog),                //
                        .grid_sh_prog   = std::move(grid_prog),                 //
                        .sprite_sh_prog = std::move(sprite_prog),               //
                        .cursor_txt     = get_texture(cursor_img),              //
                        .cursor         = std::make_unique<minicad::Cursor>(),  //
                        .camera         = std::move(camera),                    //
                        .camera_gimbal  = std::move(gimbal),                    //
                        .grid_on        = true,                                 //
                        .tess_level     = math::Vec2i(16, 16),                  //
                        .rad_minor      = 2.F,                                  //
                        .rad_major      = 4.F,                                  //
                    });
}

MiniCadApp::MiniCadApp(std::unique_ptr<Window> window, Members&& m) : Application(std::move(window)), m_(std::move(m)) {
  glPatchParameteri(GL_PATCH_VERTICES, 4);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  window_->set_event_callback<WindowResizedEvent>(class_method_as_event_callback(this, &MiniCadApp::on_resize));
  window_->set_event_callback<MouseButtonPressedEvent>(
      class_method_as_event_callback(this, &MiniCadApp::on_mouse_pressed));
  window_->set_event_callback<MouseButtonReleasedEvent>(
      class_method_as_event_callback(this, &MiniCadApp::on_mouse_released));
  window_->set_event_callback<MouseScrolledEvent>(class_method_as_event_callback(this, &MiniCadApp::on_scrolled));
  window_->set_event_callback<KeyPressedEvent>(class_method_as_event_callback(this, &MiniCadApp::on_key_pressed));
}

void MiniCadApp::render(Application::Duration /* delta */) {
  ImGui::SetNextWindowSizeConstraints(ImVec2(300, 180), ImVec2(300, 180));
  ImGui::Begin("Torus");
  ImGui::Text("FPS: %d", fps_);
  ImGui::SliderInt("Tess Level X", &m_.tess_level.x, kMinTessLevel, kMaxTessLevel);
  ImGui::SliderInt("Tess Level Y", &m_.tess_level.y, kMinTessLevel, kMaxTessLevel);
  ImGui::SliderFloat("Rad Minor", &m_.rad_minor, 0.1F, m_.rad_major);
  ImGui::SliderFloat("Rad Major", &m_.rad_major, m_.rad_minor, 5.F);
  ImGui::Checkbox("Grid", &m_.grid_on);
  ImGui::Checkbox("Ortho", &m_.use_ortho);
  ImGui::End();

  glClearColor(0.09, 0.05, 0.09, 1.F);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Render Torus
  m_.camera->set_orthographic(m_.use_ortho);
  m_.param_sh_prog->set_uniform("mMat", math::Mat4f::identity());
  m_.param_sh_prog->set_uniform("vMat", m_.camera->view_matrix());
  m_.param_sh_prog->set_uniform("pMat", m_.camera->proj_matrix());
  m_.param_sh_prog->set_uniform("tessLevelX", m_.tess_level.x);
  m_.param_sh_prog->set_uniform("tessLevelY", m_.tess_level.y);
  m_.param_sh_prog->set_uniform("minorRad", m_.rad_minor);
  m_.param_sh_prog->set_uniform("majorRad", m_.rad_major);
  m_.param_sh_prog->bind();
  m_.patch_vao.bind();
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glDrawElements(GL_PATCHES, static_cast<GLsizei>(m_.patch_vao.ebo().count()), GL_UNSIGNED_INT, nullptr);

  // Render grid
  if (m_.grid_on) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    m_.grid_sh_prog->set_uniform("u_pvMat", m_.camera->proj_matrix() * m_.camera->view_matrix());
    m_.grid_sh_prog->set_uniform("u_vInvMat", m_.camera->inverse_view_matrix());
    m_.grid_sh_prog->set_uniform("u_camWorldPos", m_.camera->transform.pos());
    m_.grid_sh_prog->bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  glDisable(GL_DEPTH_TEST);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_.cursor_txt);

  m_.sprite_sh_prog->set_uniform("u_worldPos", m_.cursor->transform.pos());
  m_.sprite_sh_prog->set_uniform("u_pvMat", m_.camera->proj_matrix() * m_.camera->view_matrix());
  m_.sprite_sh_prog->set_uniform("u_aspectRatio", m_.camera->aspect_ratio());
  m_.sprite_sh_prog->set_uniform("u_textureSampler", 0);
  m_.sprite_sh_prog->bind();
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindTexture(GL_TEXTURE_2D, 0);
  glEnable(GL_DEPTH_TEST);
}

void MiniCadApp::update(Duration delta) {
  auto deltaf = std::chrono::duration<float>(delta).count();
  m_.orbiting_camera_operator.update(*m_.camera, *m_.camera_gimbal, math::Vec2f(window_->mouse_pos()), deltaf);
  m_.cursor->update(*m_.camera, math::Vec2f(window_->mouse_pos_ndc()));
}

bool MiniCadApp::on_mouse_pressed(const MouseButtonPressedEvent& ev) {
  if (ev.is_on_ui()) {
    return false;
  }

  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonLeft) {
    m_.cursor->start_movement();
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
  m_.cursor->stop_movement();
  m_.orbiting_camera_operator.stop_rot();
  m_.orbiting_camera_operator.stop_pan();
  return true;
}

bool MiniCadApp::on_resize(const WindowResizedEvent& ev) {  // NOLINT
  glViewport(0, 0, ev.width(), ev.height());
  m_.camera->set_aspect_ratio(static_cast<float>(ev.width()) / static_cast<float>(ev.height()));
  m_.camera->recalculate_projection();
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
  if (ev.key_code() == KeyCode::G) {
    m_.grid_on = !m_.grid_on;
  }
  return false;
}

MiniCadApp::~MiniCadApp() { glDeleteTextures(1, &m_.cursor_txt); }

}  // namespace mini
