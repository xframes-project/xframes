#ifndef TEXTURE_HELPERS_H
#define TEXTURE_HELPERS_H

#ifdef __EMSCRIPTEN__
#include <webgpu/webgpu.h>
#else
#include <GLES3/gl3.h>
#endif

struct Texture {
#ifdef __EMSCRIPTEN__
    WGPUTextureView textureView;
#else
	GLuint textureView;
#endif
    int width;
    int height;
};

struct ImageJob {
    int widgetId;
    std::string url;
};

#endif //TEXTURE_HELPERS_H
