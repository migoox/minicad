#pragma once
#include <liberay/util/zstring_view.hpp>
#include <string>

namespace ImGui {  // NOLINT

namespace mini {

void OpenModal(zstring_view modal_name);

/**
 * @brief
 *
 * @param modal_name
 * @param name_holder
 * @return true when closed and name is changed
 * @return false
 */
bool RenameModal(zstring_view modal_name, std::string& name_holder);

/**
 * @brief
 *
 * @param modal_name
 * @param msg
 * @param ok_button_label
 * @param cancel_button_label
 * @return true when OK button is clicked.
 * @return false
 */
bool MessageOkCancelModal(zstring_view modal_name, zstring_view msg, zstring_view ok_button_label = "OK",
                          zstring_view cancel_button_label = "Cancel");

bool AddPatchSurfaceModal(zstring_view modal_name, int& x, int& y, float& size_x, float& size_y, float& r,
                          bool& cylinder);

}  // namespace mini

}  // namespace ImGui
