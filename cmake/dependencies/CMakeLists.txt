################################################################################
include(FetchContent)

set(EXPORT_COMPILE_COMMANDS_TEMP ${CMAKE_EXPORT_COMPILE_COMMANDS})
set(BUILD_SHARED_LIBS_TEMP ${BUILD_SHARED_LIBS})

set(CMAKE_EXPORT_COMPILE_COMMANDS
    OFF
    CACHE BOOL "Do not generate compile commands database for dependencies"
          FORCE)
set(BUILD_SHARED_LIBS
    OFF
    CACHE BOOL "Always build dependencies as static libraries"
          FORCE)
################################################################################

# ##############################################################################
# ImGuizmo (depends on ImGui)
# ##############################################################################
message(CHECK_START "Fetching ImGuizmo")
list(APPEND CMAKE_MESSAGE_INDENT "  ")

FetchContent_Declare(
  imguizmo 
  GIT_REPOSITORY "https://github.com/CedricGuillemet/ImGuizmo.git"
  GIT_TAG "1.83"
  PATCH_COMMAND git apply --ignore-whitespace
                "${CMAKE_CURRENT_LIST_DIR}/../../patches/imguizmo.patch"
  UPDATE_DISCONNECTED 1)
FetchContent_MakeAvailable(imguizmo)

list(POP_BACK CMAKE_MESSAGE_INDENT)
message(CHECK_PASS "fetched")

# ##############################################################################
# nlohmann/json
# ##############################################################################
message(CHECK_START "Fetching nlohmann/json")
list(APPEND CMAKE_MESSAGE_INDENT "  ")

FetchContent_Declare(
  nlohmann_json 
  GIT_REPOSITORY "https://github.com/nlohmann/json.git"
  GIT_TAG "v3.11.3"
  UPDATE_DISCONNECTED 1)
FetchContent_MakeAvailable(nlohmann_json)

list(POP_BACK CMAKE_MESSAGE_INDENT)
message(CHECK_PASS "fetched")

################################################################################
set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_TEMP} CACHE BOOL "Re-enable shared libs" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ${EXPORT_COMPILE_COMMANDS_TEMP} CACHE BOOL "Re-enable compile commands for the main project" FORCE)
################################################################################
