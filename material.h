#pragma once

#include <GLES3/gl3.h>

#include "math3d.h"

constexpr int kSpectralSampleCount = 6;

enum class MaterialType {
    AluminumFoil,
    CoatedPlastic,
    CoatedMetal,
    Copper,
    Gold,
    Silver
};

struct MaterialStructure {
    float atomSpacingNm;
    float grooveSpacingNm;
    float grooveDepthNm;
    float layerThicknessNm;
    float disorderStrength;
};

struct OpticalConstants {
    float wavelengthsNm[kSpectralSampleCount];
    float eta[kSpectralSampleCount];
    float extinction[kSpectralSampleCount];
};

struct Material {
    MaterialType type;
    Vec3 baseColor;
    float roughness;
    float specularStrength;
    float reflectivity;
    MaterialStructure structure;
    OpticalConstants optical;
};

Material createMaterial(MaterialType type);
Material createDefaultMetalMaterial();
const char* materialName(MaterialType type);
void uploadMaterial(
    GLint baseColorUniform,
    GLint roughnessUniform,
    GLint specularStrengthUniform,
    GLint reflectivityUniform,
    GLint structureUniform,
    GLint spectralWavelengthsUniform,
    GLint opticalEtaUniform,
    GLint opticalExtinctionUniform,
    const Material& material
);
