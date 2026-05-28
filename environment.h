#pragma once

#include <GLES3/gl3.h>

#include "math3d.h"

struct Environment {
    Vec3 lowColor;
    Vec3 highColor;
    Vec3 horizonColor;
    Vec3 softboxColor;
    float horizonStrength;
    float brightPanelStrength;
    float softboxStrength;
    float darkBandStrength;
};

Environment createDefaultEnvironment();
void uploadEnvironment(
    GLint lowColorUniform,
    GLint highColorUniform,
    GLint horizonColorUniform,
    GLint softboxColorUniform,
    GLint controlsUniform,
    const Environment& environment
);
