#pragma once

#include <GLES3/gl3.h>

#include "math3d.h"

struct DirectionalLight {
    Vec3 direction;
};

DirectionalLight createDefaultDirectionalLight();
void uploadDirectionalLight(GLint directionUniform, const DirectionalLight& light);
