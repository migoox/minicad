cmake_minimum_required(VERSION 3.20)

project(
  minicad 
  DESCRIPTION "MiNI CAD"
  LANGUAGES CXX C
  )

option(GENERATE_CLANGD_FILE
  "Generate the .clangd file for the host operating system" ON)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

# Print version
set(VERSION_SUFFIX "")
if(IS_STABLE EQUAL 0)
  set(VERSION_SUFFIX "(unstable)")
endif()
message(STATUS "Version: ${CMAKE_PROJECT_VERSION}${VERSION_SUFFIX}")


# Set the binaries and libraries output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include CTest in order to generate `DartConfiguration.tcl`
include(CTest)

if(GENERATE_CLANGD_FILE)
  include(cmake/generate_clangd_file.cmake)
  generate_clangd_file("${PROJ_CXX_FLAGS}")
endif()

add_subdirectory(eray)
add_subdirectory(minicad)
