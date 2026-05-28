#pragma once

#include <GLES3/gl3.h>

#include "math3d.h"

struct DirectionalLight {
    Vec3 direction;
};

constexpr int kLightPresetCount = 4;

DirectionalLight createDefaultDirectionalLight();
DirectionalLight createDirectionalLightPreset(int index);
const char* lightPresetName(int index);
void uploadDirectionalLight(GLint directionUniform, const DirectionalLight& light);
