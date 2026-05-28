#pragma once

#include <GLES3/gl3.h>

#include "math3d.h"

enum class MaterialType {
    AluminumFoil,
    CoatedPlastic,
    CoatedMetal
};

struct MaterialStructure {
    float atomSpacingNm;
    float grooveSpacingNm;
    float grooveDepthNm;
    float layerThicknessNm;
    float disorderStrength;
};

struct Material {
    MaterialType type;
    Vec3 baseColor;
    float roughness;
    float specularStrength;
    float reflectivity;
    MaterialStructure structure;
};

Material createDefaultMetalMaterial();
void uploadMaterial(
    GLint baseColorUniform,
    GLint roughnessUniform,
    GLint specularStrengthUniform,
    GLint reflectivityUniform,
    GLint structureUniform,
    const Material& material
);
