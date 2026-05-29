#include <cmath>
#include <iostream>
#include <string>

namespace {

constexpr float kEpsilon = 0.0001f;

bool near(float a, float b, float tolerance) {
    return std::fabs(a - b) <= tolerance;
}

bool expect(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << "\n";
        return false;
    }
    std::cout << "PASS: " << label << "\n";
    return true;
}

float diffractionTarget(int order, float wavelengthNm, float grooveSpacingNm) {
    return static_cast<float>(order) * wavelengthNm / grooveSpacingNm;
}

float diffractionBandWidth(float roughness, float disorderStrength) {
    const float roughDisorder = std::fmin(std::fmax(roughness + disorderStrength * 0.35f, 0.0f), 1.0f);
    return 0.010f + (0.085f - 0.010f) * roughDisorder;
}

float diffractionCoherence(float roughness, float disorderStrength) {
    const float roughDisorder = std::fmin(std::fmax(roughness + disorderStrength * 0.35f, 0.0f), 1.0f);
    const float t = std::fmin(std::fmax((roughDisorder - 0.12f) / (0.88f - 0.12f), 0.0f), 1.0f);
    const float smooth = t * t * (3.0f - 2.0f * t);
    return 1.0f + (0.28f - 1.0f) * smooth;
}

float springForceMagnitude(float currentLength, float restLength, float stiffness) {
    return (currentLength - restLength) * stiffness;
}

int runValidation() {
    int failures = 0;

    failures += !expect(near(diffractionTarget(1, 550.0f, 1100.0f), 0.5f, kEpsilon), "first diffraction order target uses wavelength / spacing");
    failures += !expect(std::fabs(diffractionTarget(2, 650.0f, 900.0f)) > 1.0f, "large second-order target falls outside visible angle range");
    failures += !expect(std::fabs(diffractionTarget(1, 450.0f, 900.0f)) < std::fabs(diffractionTarget(1, 650.0f, 900.0f)), "longer wavelengths diffract farther than shorter wavelengths");

    const float smoothBand = diffractionBandWidth(0.05f, 0.0f);
    const float roughBand = diffractionBandWidth(0.65f, 0.4f);
    failures += !expect(roughBand > smoothBand, "roughness widens diffraction bands");
    failures += !expect(diffractionCoherence(0.65f, 0.4f) < diffractionCoherence(0.05f, 0.0f), "roughness lowers diffraction coherence");

    failures += !expect(springForceMagnitude(1.2f, 1.0f, 95.0f) > 0.0f, "stretched spring pulls back toward rest length");
    failures += !expect(springForceMagnitude(0.8f, 1.0f, 95.0f) < 0.0f, "compressed spring pushes away from rest length");

    return failures;
}

}

int main() {
    const int failures = runValidation();
    if (failures > 0) {
        std::cerr << failures << " validation check(s) failed.\n";
        return 1;
    }

    std::cout << "All validation checks passed.\n";
    return 0;
}
