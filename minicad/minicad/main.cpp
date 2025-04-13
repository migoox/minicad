#include <iostream>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/os/driver.hpp>
#include <liberay/os/system.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <memory>
#include <minicad/app.hpp>

int main() {
  eray::util::Logger::instance().add_scribe(std::make_unique<eray::util::TerminalLoggerScribe>());
  eray::util::unwrap_or_panic(eray::os::System::request_driver(eray::os::Driver::OpenGL));

  auto window = eray::util::unwrap_or_panic(eray::os::System::instance().create_window({
      .title         = "MiNI CAD",
      .vsync         = true,
      .fullscreen    = false,
      .size          = eray::math::Vec2i(1280, 720),
      .has_valid_pos = false,
  }));

  auto app = mini::MiniCadApp::create(std::move(window));
  app.run();

  return 0;
}
