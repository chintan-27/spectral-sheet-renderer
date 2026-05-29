#include "shaders.h"

const char* vertexShaderSource = R"(#version 300 es
layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;
uniform sampler2D uClothPositionTex;
uniform vec2 uClothResolution;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vTangent;
out vec3 vBitangent;
out vec3 vGrooveDir;

vec3 clothPosition(vec2 uv) {
    vec2 halfTexel = 0.5 / uClothResolution;
    return texture(uClothPositionTex, clamp(uv, halfTexel, 1.0 - halfTexel)).xyz;
}

void main() {
    vec2 uv = aPos.xz * 0.5 + 0.5;
    vec2 texel = 1.0 / uClothResolution;
    vec3 displacedPos = clothPosition(uv);
    vec3 tangent = normalize(clothPosition(uv + vec2(texel.x, 0.0)) - clothPosition(uv - vec2(texel.x, 0.0)));
    vec3 bitangent = normalize(clothPosition(uv + vec2(0.0, texel.y)) - clothPosition(uv - vec2(0.0, texel.y)));
    vec3 normal = normalize(cross(bitangent, tangent));

    vWorldPos = displacedPos;
    vNormal = normal;
    vTangent = tangent;
    vBitangent = bitangent;
    vGrooveDir = bitangent;
    gl_Position = uMVP * vec4(displacedPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(#version 300 es
precision mediump float;

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vTangent;
in vec3 vBitangent;
in vec3 vGrooveDir;
uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform vec3 uMaterialBaseColor;
uniform float uMaterialRoughness;
uniform float uMaterialSpecularStrength;
uniform float uMaterialReflectivity;
uniform vec3 uMaterialEffects;
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

vec3 thinFilmTint(float layerThicknessNm, float cosTheta) {
    float layerAmount = clamp(layerThicknessNm / 120.0, 0.0, 1.0);
    if (layerAmount <= 0.001) {
        return vec3(1.0);
    }

    vec3 tint = vec3(0.0);
    vec3 rgbWeight = vec3(0.0);
    float layerEta = 1.46;
    float angleTerm = sqrt(max(1.0 - pow(1.0 - cosTheta, 2.0) * 0.35, 0.05));

    for (int i = 0; i < 6; ++i) {
        vec3 sampleRgb = wavelengthToRgb(uSpectralWavelengths[i]);
        float phase = 12.56637 * layerEta * layerThicknessNm * angleTerm / uSpectralWavelengths[i];
        float interference = 0.5 + 0.5 * cos(phase + 3.14159);
        tint += sampleRgb * mix(0.72, 1.24, interference);
        rgbWeight += sampleRgb;
    }

    vec3 normalizedTint = tint / max(rgbWeight, vec3(0.001));
    return mix(vec3(1.0), clamp(normalizedTint, 0.65, 1.35), layerAmount);
}

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
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

float fragHash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float fragValueNoise(vec2 p) {
    vec2 cell = floor(p);
    vec2 local = fract(p);
    vec2 blend = local * local * (3.0 - 2.0 * local);

    float a = fragHash21(cell);
    float b = fragHash21(cell + vec2(1.0, 0.0));
    float c = fragHash21(cell + vec2(0.0, 1.0));
    float d = fragHash21(cell + vec2(1.0, 1.0));

    return mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
}

float fragFbm(vec2 p) {
    float total = 0.0;
    float amplitude = 0.5;

    for (int i = 0; i < 4; ++i) {
        total += fragValueNoise(p) * amplitude;
        p = p * 2.03 + vec2(17.7, 9.2);
        amplitude *= 0.5;
    }

    return total;
}

vec3 diffractionColor(
    vec3 normal,
    vec3 grooveDir,
    vec3 lightDir,
    vec3 viewDir,
    float grooveSpacingNm,
    float grooveDepthNm,
    float roughness,
    float disorderStrength
) {
    vec3 acrossGroove = normalize(cross(grooveDir, normal));
    float lightAcross = dot(lightDir, acrossGroove);
    float viewAcross = dot(viewDir, acrossGroove);
    float gratingTerm = viewAcross - lightAcross;
    float depthStrength = clamp(grooveDepthNm / 70.0, 0.0, 1.0);
    float roughDisorder = clamp(roughness + disorderStrength * 0.35, 0.0, 1.0);
    float bandWidth = mix(0.010, 0.085, roughDisorder);
    float coherence = mix(1.0, 0.28, smoothstep(0.12, 0.88, roughDisorder));

    vec3 color = vec3(0.0);
    float weight = 0.0;

    for (int i = 0; i < 6; ++i) {
        float wavelength = uSpectralWavelengths[i];
        vec3 sampleRgb = wavelengthToRgb(wavelength);

        for (int order = -2; order <= 2; ++order) {
            if (order == 0) {
                continue;
            }

            float target = float(order) * wavelength / grooveSpacingNm;
            float error = abs(gratingTerm - target);
            float band = 1.0 - smoothstep(0.0, bandWidth, error);
            float orderWeight = 1.0 / (1.0 + abs(float(order)));
            float contribution = band * orderWeight * depthStrength * coherence;
            color += sampleRgb * contribution;
            weight += contribution;
        }
    }

    if (weight <= 0.0001) {
        return vec3(0.0);
    }

    return clamp(color / weight, 0.0, 1.0) * clamp(weight * 1.4, 0.0, 1.0);
}

void main() {
    vec2 cell = abs(fract(vWorldPos.xz * 24.0) - 0.5);
    float line = 1.0 - smoothstep(0.465, 0.5, min(cell.x, cell.y));
    vec3 normal = normalize(vNormal);
    vec3 tangent = normalize(vTangent);
    vec3 bitangent = normalize(vBitangent);
    vec3 grooveDir = normalize(vGrooveDir);
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
    float grainNoise = fragFbm(vWorldPos.xz * 3.0 + vec2(4.2, 9.1));
    float spacingNoise = fragFbm(vWorldPos.xz * 7.0 + vec2(15.0, 2.7));
    float defectNoise = fragFbm(vWorldPos.xz * 13.0 + vec2(0.5, 21.0));
    float grooveTurn = (grainNoise - 0.5) * disorderStrength * 0.75;
    grooveDir = normalize(grooveDir + tangent * grooveTurn);
    float localGrooveSpacingNm = grooveSpacingNm * (1.0 + (spacingNoise - 0.5) * disorderStrength * 0.18);
    localGrooveSpacingNm = max(localGrooveSpacingNm, 1.0);
    float defectMask = mix(1.0, smoothstep(0.16, 0.72, defectNoise), disorderStrength * 0.42);

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
        vec2 acrossGroove = normalize(vec2(grooveDir.z, -grooveDir.x));
        float groovePhase = dot(vWorldPos.xz, acrossGroove) * 18.0;
        float grooveBands = smoothstep(0.42, 0.5, abs(fract(groovePhase) - 0.5));
        vec3 grooveColor = grooveDir * 0.5 + 0.5;
        FragColor = vec4(mix(normalColor, grooveColor, 0.45) + vec3(grooveBands * 0.22), 1.0);
        return;
    }

    if (uDebugView == 13) {
        vec3 tangentColor = tangent * 0.5 + 0.5;
        vec3 bitangentColor = bitangent * 0.5 + 0.5;
        FragColor = vec4(tangentColor.r, bitangentColor.g, normal.b * 0.5 + 0.5, 1.0);
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

    if (uDebugView == 12) {
        vec3 frontReflectance = spectralMaterialColor(1.0);
        vec3 currentReflectance = spectralMaterialColor(max(dot(normal, viewDir), 0.0));
        vec3 glancingReflectance = spectralMaterialColor(0.08);
        FragColor = vec4(
            luminance(frontReflectance),
            luminance(currentReflectance),
            luminance(glancingReflectance),
            1.0
        );
        return;
    }

    vec3 rainbowColor = diffractionColor(
        normal,
        grooveDir,
        lightDir,
        viewDir,
        localGrooveSpacingNm,
        grooveDepthNm,
        roughness,
        disorderStrength
    );
    rainbowColor *= defectMask;
    rainbowColor *= clamp(uMaterialEffects.x, 0.0, 1.0);

    if (uDebugView == 14) {
        FragColor = vec4(rainbowColor, 1.0);
        return;
    }

    vec3 filmTint = mix(vec3(1.0), thinFilmTint(layerThicknessNm, max(dot(normal, viewDir), 0.0)), clamp(uMaterialEffects.y, 0.0, 1.0));
    vec3 conductorColor = clamp(spectralColor * filmTint, 0.0, 1.25);
    vec3 materialColor = mix(uMaterialBaseColor, conductorColor, 0.86);
    vec3 diffuseColor = materialColor * (0.020 + diffuse * 0.065);
    vec3 layerTint = mix(vec3(1.0), vec3(0.94, 0.98, 1.04), layerBoost * 0.22);
    vec3 reflectionColor = proceduralEnvironment(normalize(reflectionDir)) * conductorColor * layerTint;
    vec3 specularColor = conductorColor * specular;
    float rainbowStrength = clamp(0.18 + grooveDepthNm / 110.0, 0.0, 0.72) * clamp(uMaterialEffects.x, 0.0, 1.0);
    float reflectionBias = clamp(reflectivity * 0.85 + fresnel * 0.25, 0.0, 1.0);
    vec3 metalColor = mix(diffuseColor, reflectionColor, reflectionBias) + specularColor;
    vec3 rainbowReflection = rainbowColor * conductorColor * (0.55 + reflectionBias * 0.65);
    vec3 litColor = clamp(metalColor + rainbowReflection * rainbowStrength, 0.0, 1.6);
    vec3 gridColor = litColor * 0.62;
    FragColor = vec4(mix(litColor, gridColor, line * 0.14), 1.0);
}
)";

const char* clothSimVertexShaderSource = R"(#version 300 es
precision highp float;

void main() {
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
)";

const char* clothSimFragmentShaderSource = R"(#version 300 es
precision highp float;

uniform sampler2D uPositionTex;
uniform sampler2D uVelocityTex;
uniform vec2 uResolution;
uniform float uDeltaTime;
uniform float uTime;
uniform vec4 uInteraction;
uniform vec3 uMaterialEffects;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outVelocity;

vec3 restPosition(vec2 uv) {
    float v = uv.y;
    return vec3((uv.x - 0.5) * 2.0, 0.86 - v * 1.76, -0.18);
}

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

vec3 fetchPosition(ivec2 coord) {
    ivec2 clampedCoord = clamp(coord, ivec2(0), ivec2(uResolution) - ivec2(1));
    return texelFetch(uPositionTex, clampedCoord, 0).xyz;
}

void addSpring(inout vec3 force, vec3 position, ivec2 coord, ivec2 offset, float restLength, float stiffness) {
    vec3 otherPosition = fetchPosition(coord + offset);
    vec3 delta = otherPosition - position;
    float lengthNow = length(delta);
    if (lengthNow > 0.0001) {
        force += normalize(delta) * (lengthNow - restLength) * stiffness;
    }
}

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    vec2 uv = (vec2(coord) + 0.5) / uResolution;
    vec3 position = texelFetch(uPositionTex, coord, 0).xyz;
    vec3 velocity = texelFetch(uVelocityTex, coord, 0).xyz;
    vec3 rest = restPosition(uv);

    if (coord.y == 0) {
        outPosition = vec4(rest, 1.0);
        outVelocity = vec4(0.0);
        return;
    }

    float dx = 2.0 / max(uResolution.x - 1.0, 1.0);
    float dy = 1.76 / max(uResolution.y - 1.0, 1.0);
    float diagonal = length(vec2(dx, dy));
    float motion = clamp(uMaterialEffects.z, 0.0, 1.0);
    float dt = min(uDeltaTime, 0.033);

    vec3 force = vec3(0.0, -2.30, 0.0);
    addSpring(force, position, coord, ivec2(1, 0), dx, 95.0);
    addSpring(force, position, coord, ivec2(-1, 0), dx, 95.0);
    addSpring(force, position, coord, ivec2(0, 1), dy, 95.0);
    addSpring(force, position, coord, ivec2(0, -1), dy, 95.0);
    addSpring(force, position, coord, ivec2(1, 1), diagonal, 32.0);
    addSpring(force, position, coord, ivec2(-1, 1), diagonal, 32.0);
    addSpring(force, position, coord, ivec2(1, -1), diagonal, 32.0);
    addSpring(force, position, coord, ivec2(-1, -1), diagonal, 32.0);
    addSpring(force, position, coord, ivec2(2, 0), dx * 2.0, 18.0);
    addSpring(force, position, coord, ivec2(0, 2), dy * 2.0, 18.0);
    addSpring(force, position, coord, ivec2(-2, 0), dx * 2.0, 18.0);
    addSpring(force, position, coord, ivec2(0, -2), dy * 2.0, 18.0);

    force += (rest - position) * mix(0.28, 0.08, motion);

    float n = hash21(vec2(coord) * 0.37 + vec2(uTime * 0.31, uTime * 0.17));
    float wind = (sin(uTime * 1.8 + uv.x * 8.0 + uv.y * 4.0) + n - 0.5) * motion;
    force += vec3(0.10 * wind, 0.05 * wind, 0.55 * wind);

    vec2 pointerUv = vec2(clamp(uInteraction.x * 0.5 + 0.5, 0.0, 1.0), clamp(uInteraction.y * 0.5 + 0.5, 0.0, 1.0));
    float pointerDistance = length(uv - pointerUv);
    float pointerFalloff = exp(-(pointerDistance * pointerDistance) / 0.018);
    force += vec3(0.0, 1.4, 4.8) * pointerFalloff * uInteraction.z;

    velocity += force * dt;
    velocity *= mix(0.992, 0.978, motion);
    position += velocity * dt;

    if (position.y < -1.18) {
        position.y = -1.18;
        velocity.y = abs(velocity.y) * 0.16;
        velocity.xz *= 0.72;
    }

    outPosition = vec4(position, 1.0);
    outVelocity = vec4(velocity, 0.0);
}
)";
