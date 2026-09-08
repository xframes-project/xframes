#ifndef TEXTURE_HELPERS_H
#define TEXTURE_HELPERS_H

#ifdef __EMSCRIPTEN__
#include <webgpu/webgpu.h>
#else
#include <GLES3/gl3.h>
#endif

struct Texture {
#ifdef __EMSCRIPTEN__
    WGPUTextureView textureView = nullptr;
#else
    GLuint textureView = 0;
#endif
    int width = 0;
    int height = 0;
};

#endif //TEXTURE_HELPERS_H
