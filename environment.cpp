#include "environment.h"

Environment createDefaultEnvironment() {
    return {
        {0.018f, 0.020f, 0.023f},
        {0.92f, 0.95f, 0.92f},
        {0.30f, 0.32f, 0.30f},
        {1.0f, 0.98f, 0.90f},
        1.0f,
        1.9f,
        1.45f,
        0.55f
    };
}

void uploadEnvironment(
    GLint lowColorUniform,
    GLint highColorUniform,
    GLint horizonColorUniform,
    GLint softboxColorUniform,
    GLint controlsUniform,
    const Environment& environment
) {
    glUniform3f(lowColorUniform, environment.lowColor.x, environment.lowColor.y, environment.lowColor.z);
    glUniform3f(highColorUniform, environment.highColor.x, environment.highColor.y, environment.highColor.z);
    glUniform3f(horizonColorUniform, environment.horizonColor.x, environment.horizonColor.y, environment.horizonColor.z);
    glUniform3f(softboxColorUniform, environment.softboxColor.x, environment.softboxColor.y, environment.softboxColor.z);
    glUniform4f(
        controlsUniform,
        environment.horizonStrength,
        environment.brightPanelStrength,
        environment.softboxStrength,
        environment.darkBandStrength
    );
}
