#include <glad/gl.h>

#include <liberay/driver/gl/gl_error.hpp>
#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <libminicad/renderer/gl/texture_array.hpp>

namespace mini::gl {

TextureArray::TextureArray(eray::driver::gl::TextureHandle&& handle, size_t width, size_t height)
    : handle_(std::move(handle)), is_free_({true}), width_(width), height_(height) {
  for (auto i = kMaxTextures - 1; i < kMaxTextures; --i) {
    free_.push(i);
  }
  std::ranges::fill(is_free_, true);
}

TextureArray TextureArray::create(size_t width, size_t height) {
  GLuint texture = 0;
  GLsizei mips   = 1;
  auto gl_width  = static_cast<GLsizei>(width);
  auto gl_height = static_cast<GLsizei>(height);

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
  ERAY_GL_CALL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, mips, GL_RGBA8, gl_width, gl_height, kMaxTextures));

  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  eray::util::Logger::info("Created TextureArray with id {}", texture);

  return TextureArray(eray::driver::gl::TextureHandle(texture), width, height);
}

SubTextureId TextureArray::upload_texture(std::span<const uint32_t> data) {
  if (data.size() != width_ * height_) {
    eray::util::panic("TextureArray width and height does not match the provided array size.");
  }

  auto z = free_.top();
  free_.pop();
  is_free_[z] = false;

  glBindTexture(GL_TEXTURE_2D_ARRAY, handle_.get());
  ERAY_GL_CALL(glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, z, width_, height_, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
                               data.data()));
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  return z;
}

void TextureArray::reupload_texture(SubTextureId id, std::span<const uint32_t> data) {
  auto z = id;
  if (is_free_[id]) {
    eray::util::panic("Cannot reupload texture to the texture array. The subtexture with id {} does not exist.", id);
  }
  glBindTexture(GL_TEXTURE_2D_ARRAY, handle_.get());
  ERAY_GL_CALL(glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, z, width_, height_, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
                               data.data()));
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void TextureArray::delete_texture(SubTextureId id) {
  free_.push(id);
  is_free_[id] = true;
}

}  // namespace mini::gl
