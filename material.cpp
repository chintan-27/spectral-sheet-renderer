#include "material.h"

#include <initializer_list>

namespace {
constexpr float kWavelengths[kSpectralSampleCount] = {430.0f, 470.0f, 510.0f, 550.0f, 590.0f, 650.0f};

OpticalConstants optical(
    std::initializer_list<float> eta,
    std::initializer_list<float> extinction
) {
    OpticalConstants constants = {};
    int i = 0;
    auto etaIt = eta.begin();
    auto extinctionIt = extinction.begin();
    for (; i < kSpectralSampleCount && etaIt != eta.end() && extinctionIt != extinction.end(); ++i, ++etaIt, ++extinctionIt) {
        constants.wavelengthsNm[i] = kWavelengths[i];
        constants.eta[i] = *etaIt;
        constants.extinction[i] = *extinctionIt;
    }

    for (; i < kSpectralSampleCount; ++i) {
        constants.wavelengthsNm[i] = kWavelengths[i];
        constants.eta[i] = 1.0f;
        constants.extinction[i] = 0.0f;
    }
    return constants;
}
}

Material createMaterial(MaterialType type) {
    switch (type) {
        case MaterialType::AluminumFoil:
            return {
                MaterialType::AluminumFoil,
                {0.74f, 0.76f, 0.73f},
                0.10f,
                1.55f,
                0.88f,
                {0.18f, 0.04f, 0.70f},
                {
                    0.286f,
                    900.0f,
                    55.0f,
                    25.0f,
                    0.18f
                },
                optical(
                    {0.44f, 0.58f, 0.72f, 0.92f, 1.15f, 1.45f},
                    {4.75f, 5.25f, 5.85f, 6.45f, 7.10f, 8.05f}
                )
            };

        case MaterialType::CoatedPlastic:
            return {
                MaterialType::CoatedPlastic,
                {0.52f, 0.62f, 0.68f},
                0.64f,
                0.32f,
                0.18f,
                {0.05f, 0.80f, 0.95f},
                {
                    0.35f,
                    1400.0f,
                    8.0f,
                    95.0f,
                    0.10f
                },
                optical(
                    {1.48f, 1.48f, 1.47f, 1.47f, 1.46f, 1.46f},
                    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}
                )
            };

        case MaterialType::CoatedMetal:
            return {
                MaterialType::CoatedMetal,
                {0.70f, 0.68f, 0.62f},
                0.16f,
                1.24f,
                0.74f,
                {1.00f, 0.72f, 0.62f},
                {
                    0.288f,
                    780.0f,
                    38.0f,
                    70.0f,
                    0.24f
                },
                optical(
                    {0.95f, 0.86f, 0.72f, 0.55f, 0.42f, 0.32f},
                    {2.35f, 2.55f, 2.85f, 3.18f, 3.55f, 4.05f}
                )
            };

        case MaterialType::Copper:
            return {
                MaterialType::Copper,
                {0.95f, 0.48f, 0.28f},
                0.075f,
                1.55f,
                0.88f,
                {0.03f, 0.02f, 0.45f},
                {
                    0.256f,
                    820.0f,
                    34.0f,
                    12.0f,
                    0.16f
                },
                optical(
                    {1.32f, 1.18f, 0.98f, 0.72f, 0.44f, 0.26f},
                    {2.05f, 2.24f, 2.48f, 2.75f, 3.10f, 3.62f}
                )
            };

        case MaterialType::Gold:
            return {
                MaterialType::Gold,
                {1.0f, 0.78f, 0.34f},
                0.06f,
                1.62f,
                0.91f,
                {0.02f, 0.02f, 0.42f},
                {
                    0.288f,
                    860.0f,
                    28.0f,
                    10.0f,
                    0.12f
                },
                optical(
                    {1.48f, 1.35f, 0.90f, 0.48f, 0.28f, 0.17f},
                    {1.80f, 1.86f, 1.98f, 2.45f, 3.15f, 3.75f}
                )
            };

        case MaterialType::Silver:
            return {
                MaterialType::Silver,
                {0.90f, 0.92f, 0.90f},
                0.04f,
                1.80f,
                0.95f,
                {0.04f, 0.02f, 0.45f},
                {
                    0.289f,
                    760.0f,
                    22.0f,
                    8.0f,
                    0.08f
                },
                optical(
                    {0.16f, 0.14f, 0.13f, 0.13f, 0.14f, 0.16f},
                    {2.35f, 2.70f, 3.05f, 3.42f, 3.76f, 4.18f}
                )
            };
        }

    return createMaterial(MaterialType::AluminumFoil);
}

Material createDefaultMetalMaterial() {
    return createMaterial(MaterialType::AluminumFoil);
}

const char* materialName(MaterialType type) {
    switch (type) {
        case MaterialType::AluminumFoil:
            return "Aluminum Foil";
        case MaterialType::CoatedPlastic:
            return "Coated Plastic";
        case MaterialType::CoatedMetal:
            return "Reflective Rainbow Sheet";
        case MaterialType::Copper:
            return "Copper";
        case MaterialType::Gold:
            return "Gold";
        case MaterialType::Silver:
            return "Silver";
    }

    return "Aluminum Foil";
}

void uploadMaterial(
    GLint baseColorUniform,
    GLint roughnessUniform,
    GLint specularStrengthUniform,
    GLint reflectivityUniform,
    GLint effectStrengthsUniform,
    GLint structureUniform,
    GLint spectralWavelengthsUniform,
    GLint opticalEtaUniform,
    GLint opticalExtinctionUniform,
    const Material& material
) {
    glUniform3f(baseColorUniform, material.baseColor.x, material.baseColor.y, material.baseColor.z);
    glUniform1f(roughnessUniform, material.roughness);
    glUniform1f(specularStrengthUniform, material.specularStrength);
    glUniform1f(reflectivityUniform, material.reflectivity);
    glUniform3f(
        effectStrengthsUniform,
        material.effectStrengths.x,
        material.effectStrengths.y,
        material.effectStrengths.z
    );
    glUniform4f(
        structureUniform,
        material.structure.grooveSpacingNm,
        material.structure.grooveDepthNm,
        material.structure.layerThicknessNm,
        material.structure.disorderStrength
    );
    glUniform1fv(spectralWavelengthsUniform, kSpectralSampleCount, material.optical.wavelengthsNm);
    glUniform1fv(opticalEtaUniform, kSpectralSampleCount, material.optical.eta);
    glUniform1fv(opticalExtinctionUniform, kSpectralSampleCount, material.optical.extinction);
}
