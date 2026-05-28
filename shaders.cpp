#include "shaders.h"

const char* vertexShaderSource = R"(#version 300 es
layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;

void main() {
    float primaryWave = sin(aPos.x * 6.0 + uTime * 1.4);
    float crossWave = sin((aPos.x + aPos.z) * 4.0 - uTime * 0.9);
    float height = primaryWave * 0.09 + crossWave * 0.04;
    float primarySlopeX = cos(aPos.x * 6.0 + uTime * 1.4) * 6.0 * 0.09;
    float crossSlope = cos((aPos.x + aPos.z) * 4.0 - uTime * 0.9) * 4.0 * 0.04;

    vec3 displacedPos = vec3(aPos.x, height, aPos.z);
    vWorldPos = displacedPos;
    vNormal = normalize(vec3(-(primarySlopeX + crossSlope), 1.0, -crossSlope));
    gl_Position = uMVP * vec4(displacedPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(#version 300 es
precision mediump float;

in vec3 vWorldPos;
in vec3 vNormal;
uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform vec3 uMaterialBaseColor;
uniform float uMaterialRoughness;
uniform float uMaterialSpecularStrength;
uniform float uMaterialReflectivity;
uniform vec4 uMaterialStructure;
uniform float uSpectralWavelengths[6];
uniform float uOpticalEta[6];
uniform float uOpticalExtinction[6];
uniform vec3 uEnvLowColor;
uniform vec3 uEnvHighColor;
uniform vec3 uEnvHorizonColor;
uniform vec3 uEnvSoftboxColor;
uniform vec4 uEnvControls;
uniform int uDebugView;
uniform int uSpectralDebugIndex;
out vec4 FragColor;

vec3 wavelengthToRgb(float wavelengthNm) {
    float r = 0.0;
    float g = 0.0;
    float b = 0.0;

    if (wavelengthNm < 440.0) {
        r = -(wavelengthNm - 440.0) / 60.0;
        b = 1.0;
    } else if (wavelengthNm < 490.0) {
        g = (wavelengthNm - 440.0) / 50.0;
        b = 1.0;
    } else if (wavelengthNm < 510.0) {
        g = 1.0;
        b = -(wavelengthNm - 510.0) / 20.0;
    } else if (wavelengthNm < 580.0) {
        r = (wavelengthNm - 510.0) / 70.0;
        g = 1.0;
    } else if (wavelengthNm < 645.0) {
        r = 1.0;
        g = -(wavelengthNm - 645.0) / 65.0;
    } else {
        r = 1.0;
    }

    float edge = 1.0;
    if (wavelengthNm < 420.0) {
        edge = 0.3 + 0.7 * (wavelengthNm - 380.0) / 40.0;
    } else if (wavelengthNm > 645.0) {
        edge = 0.3 + 0.7 * (700.0 - wavelengthNm) / 55.0;
    }

    return clamp(vec3(r, g, b) * edge, 0.0, 1.0);
}

float conductorReflectance(float cosTheta, float eta, float extinction) {
    float c = clamp(cosTheta, 0.0, 1.0);
    float eta2 = eta * eta;
    float k2 = extinction * extinction;
    float twoEtaC = 2.0 * eta * c;
    float c2 = c * c;
    float rs = ((eta2 + k2) - twoEtaC + c2) / ((eta2 + k2) + twoEtaC + c2);
    float rp = ((eta2 + k2) * c2 - twoEtaC + 1.0) / ((eta2 + k2) * c2 + twoEtaC + 1.0);
    return clamp((rs + rp) * 0.5, 0.0, 1.0);
}

vec3 spectralMaterialColor(float cosTheta) {
    vec3 spectrumRgb = vec3(0.0);
    vec3 rgbWeight = vec3(0.0);

    for (int i = 0; i < 6; ++i) {
        vec3 sampleRgb = wavelengthToRgb(uSpectralWavelengths[i]);
        float reflectance = conductorReflectance(cosTheta, uOpticalEta[i], uOpticalExtinction[i]);
        spectrumRgb += sampleRgb * reflectance;
        rgbWeight += sampleRgb;
    }

    return clamp(spectrumRgb / max(rgbWeight, vec3(0.001)), 0.0, 1.0);
}

vec3 proceduralEnvironment(vec3 dir) {
    float skyMix = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 env = mix(uEnvLowColor, uEnvHighColor, skyMix);

    float horizon = 1.0 - smoothstep(0.0, 0.18, abs(dir.y));
    env += uEnvHorizonColor * horizon * uEnvControls.x;

    float brightPanel = smoothstep(0.965, 0.995, dir.x * 0.65 + dir.y * 0.55 + dir.z * 0.25);
    float darkPanel = smoothstep(0.90, 0.98, -dir.x * 0.55 + dir.z * 0.75);
    float longSoftbox = 1.0 - smoothstep(0.025, 0.13, abs(dir.x * 0.72 - dir.z * 0.32 + 0.10));
    longSoftbox *= smoothstep(-0.25, 0.45, dir.y);
    float hardStrip = 1.0 - smoothstep(0.008, 0.035, abs(dir.x * 0.24 + dir.y * 0.84 + dir.z * 0.18 - 0.42));
    float darkStrip = 1.0 - smoothstep(0.018, 0.08, abs(dir.x * 0.62 + dir.z * 0.58 - 0.12));

    env += uEnvSoftboxColor * brightPanel * uEnvControls.y;
    env += uEnvSoftboxColor * longSoftbox * uEnvControls.z;
    env += uEnvSoftboxColor * hardStrip * (uEnvControls.z * 1.5);
    env *= 1.0 - darkPanel * uEnvControls.w;
    env *= 1.0 - darkStrip * (uEnvControls.w * 0.64);

    return env;
}

void main() {
    vec2 cell = abs(fract(vWorldPos.xz * 24.0) - 0.5);
    float line = 1.0 - smoothstep(0.465, 0.5, min(cell.x, cell.y));
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    vec3 viewDir = normalize(uCameraPos - vWorldPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    vec3 reflectionDir = reflect(-viewDir, normal);
    float diffuse = max(dot(normal, lightDir), 0.0);
    float grooveSpacingNm = max(uMaterialStructure.x, 1.0);
    float grooveDepthNm = max(uMaterialStructure.y, 0.0);
    float layerThicknessNm = max(uMaterialStructure.z, 0.0);
    float disorderStrength = clamp(uMaterialStructure.w, 0.0, 1.0);
    float grooveScale = clamp(grooveDepthNm / grooveSpacingNm, 0.0, 0.25);
    float layerBoost = clamp(layerThicknessNm / 120.0, 0.0, 1.0);
    float roughness = clamp(uMaterialRoughness + disorderStrength * 0.035 + grooveScale * 0.08, 0.015, 1.0);

    if (uDebugView == 1) {
        float value = clamp((grooveSpacingNm - 500.0) / 1200.0, 0.0, 1.0);
        FragColor = vec4(value, 0.18, 1.0 - value, 1.0);
        return;
    }

    if (uDebugView == 2) {
        float value = clamp(grooveDepthNm / 80.0, 0.0, 1.0);
        FragColor = vec4(value, value * 0.75, 0.08, 1.0);
        return;
    }

    if (uDebugView == 3) {
        FragColor = vec4(vec3(layerBoost), 1.0);
        return;
    }

    if (uDebugView == 4) {
        FragColor = vec4(disorderStrength, roughness, 1.0 - roughness, 1.0);
        return;
    }

    if (uDebugView == 5) {
        float height = clamp(vWorldPos.y * 3.8 + 0.5, 0.0, 1.0);
        FragColor = vec4(height, 0.25, 1.0 - height, 1.0);
        return;
    }

    if (uDebugView == 6) {
        vec3 normalColor = normal * 0.5 + 0.5;
        float grooveBands = smoothstep(0.42, 0.5, abs(fract(vWorldPos.x * 18.0) - 0.5));
        FragColor = vec4(mix(normalColor, vec3(1.0, 1.0, 0.1), grooveBands * 0.55), 1.0);
        return;
    }

    if (uDebugView == 7) {
        FragColor = vec4(proceduralEnvironment(normalize(reflectionDir)), 1.0);
        return;
    }

    if (uDebugView == 8) {
        FragColor = vec4(reflectionDir * 0.5 + 0.5, 1.0);
        return;
    }

    float shininess = mix(320.0, 24.0, roughness);
    float specular = pow(max(dot(normal, halfDir), 0.0), shininess) * uMaterialSpecularStrength * 1.25;
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);
    float reflectivity = clamp(uMaterialReflectivity + fresnel * 0.18, 0.0, 1.0);
    vec3 spectralColor = spectralMaterialColor(max(dot(normal, viewDir), 0.0));

    if (uDebugView == 9) {
        FragColor = vec4(spectralColor, 1.0);
        return;
    }

    if (uDebugView == 10) {
        float avgEta = 0.0;
        float avgExtinction = 0.0;
        for (int i = 0; i < 6; ++i) {
            avgEta += uOpticalEta[i];
            avgExtinction += uOpticalExtinction[i];
        }
        avgEta /= 6.0;
        avgExtinction /= 6.0;
        FragColor = vec4(clamp(avgEta / 2.0, 0.0, 1.0), clamp(avgExtinction / 5.0, 0.0, 1.0), spectralColor.b, 1.0);
        return;
    }

    if (uDebugView == 11) {
        int index = clamp(uSpectralDebugIndex, 0, 5);
        vec3 sampleRgb = wavelengthToRgb(uSpectralWavelengths[index]);
        float sampleReflectance = conductorReflectance(max(dot(normal, viewDir), 0.0), uOpticalEta[index], uOpticalExtinction[index]);
        FragColor = vec4(sampleRgb * sampleReflectance, 1.0);
        return;
    }

    vec3 materialColor = mix(uMaterialBaseColor, spectralColor, 0.68);
    vec3 diffuseColor = materialColor * (0.025 + diffuse * 0.08);
    vec3 layerTint = mix(vec3(1.0), vec3(0.94, 0.98, 1.04), layerBoost * 0.22);
    vec3 reflectionColor = proceduralEnvironment(normalize(reflectionDir)) * materialColor * layerTint;
    vec3 litColor = mix(diffuseColor, reflectionColor, reflectivity) + vec3(specular);
    vec3 gridColor = litColor * 0.62;
    FragColor = vec4(mix(litColor, gridColor, line * 0.14), 1.0);
}
)";
