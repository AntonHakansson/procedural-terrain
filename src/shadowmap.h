#include <glad/glad.h>

#include <string>
#include "fbo.h"

enum ShadowClampMode
{
  Edge = 1,
  Border = 2
};

class ShadowMap {
public:
  int resolution = 4096;
  int clamp_mode = ShadowClampMode::Border;
  bool clamp_border_shadowed = false;
  bool use_polygon_offset = true;
  bool use_soft_falloff = false;
  bool use_hardware_pcfg = false;
  float polygon_offset_factor = 1.125f;
  float polygon_offset_units = 2.0f;
  FboInfo fbo;

  ShadowMap(void);

  // init shadow map
  void init();

// bind shadow map
  void bindColorBuffer(GLuint texture);
  void bindDepthBuffer(GLuint texture);
  void bind();

  // render shadow map
  void begin();
  void end();
};
