#pragma once

#include <array>
#include <liberay/driver/gl/gl_error.hpp>
#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/ruleof.hpp>
#include <stack>

namespace mini::gl {

using SubTextureId = uint32_t;

class TextureArray {
 public:
  // Based on https://www.khronos.org/opengl/wiki/Array_Texture#Limitations
  // Assuming OpenGL version >= 4.5
  static constexpr auto kMaxTextures = 2048U;

  ERAY_DEFAULT_MOVE(TextureArray)
  ERAY_DELETE_COPY(TextureArray)

  static TextureArray create(size_t width, size_t height);
  SubTextureId upload_texture(std::span<const uint32_t> data);
  void reupload_texture(SubTextureId id, std::span<const uint32_t> data);
  void delete_texture(SubTextureId id);
  bool is_valid(SubTextureId id) const { return !is_free_[id]; }

  void bind() const { glBindTexture(GL_TEXTURE_2D_ARRAY, handle_.get()); }

  size_t width() const { return width_; }
  size_t height() const { return height_; }

 private:
  TextureArray(eray::driver::gl::TextureHandle&& handle, size_t width, size_t height);

 private:
  eray::driver::gl::TextureHandle handle_;
  std::stack<uint32_t> free_;
  std::array<bool, kMaxTextures> is_free_;
  size_t width_;
  size_t height_;
};

}  // namespace mini::gl
