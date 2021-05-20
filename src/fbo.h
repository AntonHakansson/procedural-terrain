#include <glad/glad.h>

#include <array>
#include <vector>

#include "gpu.h"

class FboInfo {
public:
  GLuint framebufferId;
  std::vector<GLuint> colorTextureTargets;
  GLuint depthBuffer;
  int width;
  int height;
  bool isComplete;

  FboInfo(int numberOfColorBuffers = 1);

  void resize(int w, int h);
  bool checkFramebufferComplete() const;
};
