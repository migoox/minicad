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
#include <liberay/util/iterator.hpp>
#include <liberay/util/timer.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <memory>
#include <minicad/camera/orbiting_camera_operator.hpp>
#include <minicad/cursor/cursor.hpp>
#include <minicad/selection/selection.hpp>
#include <minicad/tools/select_tool.hpp>

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
  enum class ToolState : uint8_t {
    Select    = 0,
    Cursor    = 1,
    Transform = 2,
  };

  struct Members {
    mini::OrbitingCameraOperator orbiting_camera_operator;

    std::unique_ptr<mini::Cursor> cursor;

    std::unique_ptr<mini::Camera> camera;
    std::unique_ptr<eray::math::Transform3f> camera_gimbal;

    bool show_centroid;

    bool grid_on;
    bool use_ortho;

    bool is_gizmo_used;

    SelectTool select_tool;
    Scene scene;

    // TODO(migoox): state machine
    ToolState tool_state;

    std::unique_ptr<SceneObjectsSelection> selection;
    std::unique_ptr<PointListObjectsSelection> point_list_selection;

    SceneRenderer scene_renderer;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> screen_quad_sh_prog;
    std::unique_ptr<eray::driver::gl::ViewportFramebuffer> viewport_fb;
  };

  MiniCadApp(std::unique_ptr<eray::os::Window> window, Members&& m);

  // GUI
  void gui_objects_list_window();
  void gui_selection_window();
  void gui_point_list_window();

  // GUI Events
  bool on_scene_object_created(SceneObjectVariant variant);
  bool on_point_created_in_point_list(const PointListObjectHandle& handle);
  bool on_scene_object_deleted(const SceneObjectHandle& handle);

  bool on_point_list_object_added(PointListObjectVariant variant);
  bool on_point_list_object_deleted(const PointListObjectHandle& handle);

  bool on_points_reorder(const PointListObjectHandle& handle, const std::optional<SceneObjectHandle>& source,
                         const std::optional<SceneObjectHandle>& before_dest,
                         const std::optional<SceneObjectHandle>& after_dest);

  bool on_cursor_state_set();
  bool on_select_state_set();
  bool on_transform_state_set();
  bool on_tool_action_start();
  bool on_tool_action_end();

  bool on_selection_add(const SceneObjectHandle& handle);

  template <eray::util::Iterator<SceneObjectHandle> It>
  bool on_selection_add_many(It begin, It end) {
    for (const auto& handle : std::ranges::subrange(begin, end)) {
      m_.scene_renderer.update_visibility_state(handle, VisibilityState::Selected);
      if (auto o = m_.scene.get_obj(handle)) {
        o.value()->mark_dirty();
      }
    }
    m_.selection->add_many(m_.scene, begin, end);
    return true;
  }

  template <eray::util::Iterator<SceneObjectHandle> It>
  bool on_selection_remove_many(It begin, It end) {
    for (const auto& handle : std::ranges::subrange(begin, end)) {
      m_.scene_renderer.update_visibility_state(handle, VisibilityState::Visible);
      if (auto o = m_.scene.get_obj(handle)) {
        o.value()->mark_dirty();
      }
    }
    m_.selection->remove_many(m_.scene, begin, end);
    return true;
  }

  bool on_selection_remove(const SceneObjectHandle& handle);
  bool on_selection_clear();

  bool on_point_list_selection_add(const PointListObjectHandle& handle);
  bool on_point_list_selection_remove(const PointListObjectHandle& handle);
  bool on_point_list_selection_clear();

  // Window Events
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
