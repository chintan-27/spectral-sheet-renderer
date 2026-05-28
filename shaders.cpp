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
out vec4 FragColor;

void main() {
    vec2 cell = abs(fract(vWorldPos.xz * 24.0) - 0.5);
    float line = 1.0 - smoothstep(0.465, 0.5, min(cell.x, cell.y));
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    float diffuse = max(dot(normal, lightDir), 0.0);

    vec3 materialColor = vec3(0.44, 0.56, 0.62);
    vec3 ambient = materialColor * 0.28;
    vec3 litColor = ambient + materialColor * diffuse * 0.82;
    vec3 gridColor = vec3(0.08, 0.12, 0.15);
    FragColor = vec4(mix(litColor, gridColor, line * 0.55), 1.0);
}
)";
