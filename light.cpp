#include "light.h"

DirectionalLight createDefaultDirectionalLight() {
    return {{-0.35f, 0.85f, 0.42f}};
}

void uploadDirectionalLight(GLint directionUniform, const DirectionalLight& light) {
    glUniform3f(directionUniform, light.direction.x, light.direction.y, light.direction.z);
}
