#include "material.h"

Material createDefaultMetalMaterial() {
    return {
        MaterialType::AluminumFoil,
        {0.74f, 0.76f, 0.73f},
        0.16f,
        1.35f,
        0.82f,
        {
            0.286f,
            900.0f,
            55.0f,
            25.0f,
            0.18f
        }
    };
}

void uploadMaterial(
    GLint baseColorUniform,
    GLint roughnessUniform,
    GLint specularStrengthUniform,
    GLint reflectivityUniform,
    GLint structureUniform,
    const Material& material
) {
    glUniform3f(baseColorUniform, material.baseColor.x, material.baseColor.y, material.baseColor.z);
    glUniform1f(roughnessUniform, material.roughness);
    glUniform1f(specularStrengthUniform, material.specularStrength);
    glUniform1f(reflectivityUniform, material.reflectivity);
    glUniform4f(
        structureUniform,
        material.structure.grooveSpacingNm,
        material.structure.grooveDepthNm,
        material.structure.layerThicknessNm,
        material.structure.disorderStrength
    );
}
