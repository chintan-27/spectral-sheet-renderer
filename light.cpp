#include "light.h"

DirectionalLight createDefaultDirectionalLight() {
    return createDirectionalLightPreset(0);
}

DirectionalLight createDirectionalLightPreset(int index) {
    switch (index % kLightPresetCount) {
        case 0:
            return {{-0.35f, 0.85f, 0.42f}};
        case 1:
            return {{0.72f, 0.52f, 0.46f}};
        case 2:
            return {{-0.20f, 0.36f, -0.91f}};
        case 3:
            return {{0.05f, 0.98f, 0.18f}};
    }

    return {{-0.35f, 0.85f, 0.42f}};
}

const char* lightPresetName(int index) {
    switch (index % kLightPresetCount) {
        case 0:
            return "Soft diagonal";
        case 1:
            return "Warm side";
        case 2:
            return "Low glancing";
        case 3:
            return "Overhead";
    }

    return "Soft diagonal";
}

void uploadDirectionalLight(GLint directionUniform, const DirectionalLight& light) {
    const Vec3 direction = normalize(light.direction);
    glUniform3f(directionUniform, direction.x, direction.y, direction.z);
}
