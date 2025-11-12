#pragma once

#include <liberay/util/object_handle.hpp>
#include <libminicad/algorithm/intersection_finder.hpp>
#include <libminicad/renderer/gl/texture_array.hpp>
#include <libminicad/scene/handles.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/types.hpp>

namespace mini::gl {

template <typename TObject>
  requires CParametricSurfaceObject<TObject> && CObject<TObject>
class TrimmingTexturesManager {
 public:
  static TrimmingTexturesManager<TObject> create() {
    return TrimmingTexturesManager<TObject>(TextureArray::create(ParamSpaceTrimmingDataManager::kGPUTrimmingTxtSize,
                                                                 ParamSpaceTrimmingDataManager::kGPUTrimmingTxtSize));
  }

  void update(TObject& obj) {
    auto it = patch_trimming_txts_.find(obj.handle());

    if (it == patch_trimming_txts_.end()) {
      auto id = trimming_txt_array_.upload_texture(obj.trimming_manager().final_txt_gpu());
      patch_trimming_txts_.emplace(obj.handle(), id);
    } else {
      trimming_txt_array_.reupload_texture(it->second, obj.trimming_manager().final_txt_gpu());
    }
  }

  void remove(const eray::util::Handle<TObject>& handle) {
    auto it = patch_trimming_txts_.find(handle);
    if (it == patch_trimming_txts_.end()) {
      return;
    }

    trimming_txt_array_.delete_texture(it->second);
    patch_trimming_txts_.erase(it);
  }

  void remove(TObject& obj) { remove(obj.handle()); }

  /**
   * @brief In the case the id is nonexistent, the texture is created.
   *
   * @param handle
   * @return SubTextureId
   */
  SubTextureId get_id(TObject& obj) {
    auto it = patch_trimming_txts_.find(obj.handle());
    if (it == patch_trimming_txts_.end()) {
      update(obj);
      return patch_trimming_txts_.at(obj.handle());
    }
    return it->second;
  }

  const TextureArray& txt_array() const { return trimming_txt_array_; }

 private:
  explicit TrimmingTexturesManager(gl::TextureArray&& txt_array) : trimming_txt_array_(std::move(txt_array)) {}

 private:
  TextureArray trimming_txt_array_;
  std::unordered_map<eray::util::Handle<TObject>, SubTextureId> patch_trimming_txts_;
};

}  // namespace mini::gl
