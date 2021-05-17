#include <glad/glad.h>

#include <string>
#include <vector>

namespace gpu {
  GLuint loadHdrTexture(const std::string &filename);
  GLuint loadHdrMipmapTexture(const std::vector<std::string> &filenames);
}  // namespace labhelper
