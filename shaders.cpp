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
out vec4 FragColor;

vec3 proceduralEnvironment(vec3 dir) {
    float skyMix = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 low = vec3(0.05, 0.055, 0.06);
    vec3 high = vec3(0.82, 0.86, 0.84);
    vec3 env = mix(low, high, skyMix);

    float horizon = 1.0 - smoothstep(0.0, 0.18, abs(dir.y));
    env += vec3(0.22, 0.24, 0.22) * horizon;

    float brightPanel = smoothstep(0.965, 0.995, dir.x * 0.65 + dir.y * 0.55 + dir.z * 0.25);
    float darkPanel = smoothstep(0.90, 0.98, -dir.x * 0.55 + dir.z * 0.75);
    env += vec3(1.0, 0.96, 0.86) * brightPanel * 1.4;
    env *= 1.0 - darkPanel * 0.45;

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
    float roughness = clamp(uMaterialRoughness + disorderStrength * 0.08 + grooveScale * 0.18, 0.02, 1.0);
    float shininess = mix(180.0, 18.0, roughness);
    float specular = pow(max(dot(normal, halfDir), 0.0), shininess) * uMaterialSpecularStrength;
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);
    float reflectivity = clamp(uMaterialReflectivity + fresnel * 0.18, 0.0, 1.0);

    vec3 materialColor = uMaterialBaseColor;
    vec3 diffuseColor = materialColor * (0.05 + diffuse * 0.16);
    vec3 layerTint = mix(vec3(1.0), vec3(0.94, 0.98, 1.04), layerBoost * 0.22);
    vec3 reflectionColor = proceduralEnvironment(normalize(reflectionDir)) * materialColor * layerTint;
    vec3 litColor = mix(diffuseColor, reflectionColor, reflectivity) + vec3(specular);
    vec3 gridColor = litColor * 0.42;
    FragColor = vec4(mix(litColor, gridColor, line * 0.24), 1.0);
}
)";
