cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 23)

project(libminicad CXX C)
message(STATUS "Configuring " ${PROJECT_NAME})
file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.cpp"
)

add_library(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC liberay nlohmann_json)

target_compile_options(${PROJECT_NAME} PRIVATE ${PROJ_CXX_FLAGS})
target_link_options(${PROJECT_NAME} PRIVATE ${PROJ_SHARED_LINKER_FLAGS})
target_include_directories(${PROJECT_NAME}
        PUBLIC . "${CMAKE_BINARY_DIR}/generated")

if(BUILD_TESTS)
  enable_testing()

  file(GLOB_RECURSE TESTS_SOURCES 
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
  )

  add_executable(
    "${PROJECT_NAME}_tests"
    ${TESTS_SOURCES} 
  )
  target_link_libraries(
    "${PROJECT_NAME}_tests"
    GTest::gtest_main
    ${PROJECT_NAME}
  )

  include(CTest)
  include(GoogleTest)
  gtest_discover_tests("${PROJECT_NAME}_tests")
endif()
