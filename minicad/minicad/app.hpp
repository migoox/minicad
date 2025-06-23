#pragma once

#include <filesystem>
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
#include <libminicad/algorithm/hole_finder.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/fill_in_suface.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <memory>
#include <minicad/camera/orbiting_camera_operator.hpp>
#include <minicad/cursor/cursor.hpp>
#include <minicad/imgui/modals.hpp>
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
    Select = 0,
    Cursor = 1,
  };

  struct Members {
    std::optional<std::filesystem::path> proj_path;

    mini::OrbitingCameraOperator orbiting_camera_operator;

    std::unique_ptr<mini::Cursor> cursor;

    std::unique_ptr<mini::Camera> camera;
    std::unique_ptr<eray::math::Transform3f> camera_gimbal;

    bool show_centroid;

    bool grid_on;
    bool use_ortho;

    bool is_gizmo_used;

    bool ctrl_pressed;

    SelectTool select_tool;
    Scene scene;

    // TODO(migoox): state machine
    ToolState tool_state;

    std::unique_ptr<SceneObjectsSelection> scene_obj_selection;
    std::unique_ptr<NonSceneObjectSelection> non_scene_obj_selection;
    HelperPointSelection helper_point_selection;
  };

  MiniCadApp(std::unique_ptr<eray::os::Window> window, Members&& m);

  // HELPERS
  std::optional<ObjectHandle> get_single_handle_selection() const;

  // GUI
  void gui_objects_list_window();
  void gui_transform_window();
  void gui_object_window();

  // GUI Events
  bool on_scene_object_added(SceneObjectVariant variant);
  bool on_point_created_in_point_list(const CurveHandle& handle);
  bool on_curve_added(CurveVariant variant);
  bool on_curve_added_from_points_selection(CurveVariant variant);
  bool on_patch_surface_added(PatchSurfaceVariant variant, const ImGui::mini::PatchSurfaceInfo& info);
  bool on_patch_surface_added_from_curve(const CurveHandle& curve_handle, const ImGui::mini::PatchSurfaceInfo& info);
  bool on_fill_in_surface_added(FillInSurfaceVariant variant, const BezierHole3Finder::BezierHole& hole);

  bool on_obj_deleted(const ObjectHandle& handle);

  bool on_curve_deleted(const CurveHandle& handle);
  bool on_patch_surface_deleted(const PatchSurfaceHandle& handle);
  bool on_selection_deleted();

  bool on_points_reorder(const CurveHandle& handle, const std::optional<size_t>& source,
                         const std::optional<size_t>& before_dest, const std::optional<size_t>& after_dest);

  bool on_selection_merge();

  bool on_cursor_state_set();
  bool on_select_state_set();
  bool on_tool_action_start();
  bool on_tool_action_end();

  template <eray::util::Iterator<SceneObjectHandle> It>
  bool on_selection_add_many(It begin, It end) {
    for (const auto& handle : std::ranges::subrange(begin, end)) {
      m_.scene.renderer().push_object_rs_cmd(
          SceneObjectRSCommand(handle, SceneObjectRSCommand::UpdateObjectVisibility(VisibilityState::Selected)));
    }
    m_.scene_obj_selection->add_many(m_.scene, begin, end);
    return true;
  }

  template <eray::util::Iterator<SceneObjectHandle> It>
  bool on_selection_remove_many(It begin, It end) {
    for (const auto& handle : std::ranges::subrange(begin, end)) {
      m_.scene.renderer().push_object_rs_cmd(
          SceneObjectRSCommand(handle, SceneObjectRSCommand::UpdateObjectVisibility(VisibilityState::Visible)));
    }
    m_.scene_obj_selection->remove_many(m_.scene, begin, end);
    return true;
  }

  bool on_selection_add(const ObjectHandle& handle);
  bool on_selection_remove(const ObjectHandle& handle);
  bool on_selection_set_single(const ObjectHandle& handle);
  bool on_selection_clear();

  bool on_fill_in_surface_from_selection();

  bool on_project_open(const std::filesystem::path& path);
  bool on_project_save_as(const std::filesystem::path& path);
  bool on_project_save();

  bool on_find_intersection();

  // Window Events
  bool on_mouse_pressed(const eray::os::MouseButtonPressedEvent& ev);
  bool on_mouse_released(const eray::os::MouseButtonReleasedEvent& ev);
  bool on_resize(const eray::os::WindowResizedEvent& ev);
  bool on_scrolled(const eray::os::MouseScrolledEvent& ev);
  bool on_key_pressed(const eray::os::KeyPressedEvent& ev);
  bool on_key_released(const eray::os::KeyReleasedEvent& ev);

 private:
  static constexpr int kMinTessLevel = 3;
  static constexpr int kMaxTessLevel = 64;

  Members m_;
};

}  // namespace mini
