#include <algorithm>
#include <cmath>
#include <vector>

#include <GLES3/gl3.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "environment.h"
#include "gpu.h"
#include "light.h"
#include "material.h"
#include "math3d.h"
#include "shaders.h"
#include "sheet_mesh.h"

EM_JS(void, updateOverlay, (
    const char* materialNamePtr,
    const char* lightNamePtr,
    const char* qualityNamePtr,
    const char* polarizationNamePtr,
    int debugView,
    float roughness,
    float reflectivity,
    float diffractionStrength,
    float filmStrength,
    float motionStrength,
    float grooveSpacingNm,
    float grooveDepthNm,
    float layerThicknessNm,
    float disorderStrength,
    float interactionStrength,
    int spectralIndex,
    float spectralWavelengthNm
), {
    const overlay = document.getElementById('debug-overlay');
    if (!overlay) {
        return;
    }

    const materialName = UTF8ToString(materialNamePtr);
    const lightName = UTF8ToString(lightNamePtr);
    const qualityName = UTF8ToString(qualityNamePtr);
    const polarizationName = UTF8ToString(polarizationNamePtr);
    const debugNames = [
        'Shaded render',
        'Groove spacing',
        'Groove depth',
        'Layer thickness',
        'Disorder / roughness',
        'Wave height',
        'Normals + groove bands',
        'Environment reflection',
        'Reflection direction',
        'Spectral material color',
        'Optical n/k summary',
        'Single wavelength',
        'Angle reflectance',
        'Tangent frame',
        'Diffraction only'
    ];
    const debugName = debugNames[debugView] || 'Unknown';

    const target = materialName === 'Reflective Rainbow Sheet' ? 'Goal rainbow sheet' : 'Ordinary material';
    const meter = function(label, value) {
        const pct = Math.max(0, Math.min(100, value * 100));
        return '<div class="meter"><span>' + label + '</span><b>' + value.toFixed(2) + '</b><i><em style="width:' + pct + '%"></em></i></div>';
    };

    overlay.innerHTML =
        '<div class="panel-head"><div><div class="title">Spectral Sheet</div><div class="subtitle">' + target + '</div></div><div class="badge">' + debugName + '</div></div>' +
        '<div class="grid">' +
        '<div><span>Material</span><strong>' + materialName + '</strong></div>' +
        '<div><span>Light</span><strong>' + lightName + '</strong></div>' +
        '<div><span>Quality</span><strong>' + qualityName + '</strong></div>' +
        '<div><span>Polarization</span><strong>' + polarizationName + '</strong></div>' +
        '<div><span>Groove</span><strong>' + grooveSpacingNm.toFixed(0) + ' nm</strong></div>' +
        '</div>' +
        meter('Reflectivity', reflectivity) +
        meter('Roughness', roughness) +
        meter('Diffraction', diffractionStrength) +
        meter('Thin film', filmStrength) +
        meter('Motion', motionStrength) +
        '<div class="readout">Depth ' + grooveDepthNm.toFixed(1) + ' nm | Disorder ' + disorderStrength.toFixed(2) + ' | Pull ' + interactionStrength.toFixed(2) + ' | Sample #' + spectralIndex + ' ' + spectralWavelengthNm.toFixed(0) + ' nm</div>' +
        '<div class="help">1 Al, 2 plastic, 3 rainbow sheet, 4 Cu, 5 Au, 6 Ag | L light | K quality | Z polarization | F reset cloth | Shift+drag pull</div>';
});

GLuint gProgram = 0;
GLuint gClothSimProgram = 0;
GLuint gClothSimVao = 0;
GLuint gClothFramebuffer = 0;
GLuint gClothPositionTextures[2] = {0, 0};
GLuint gClothVelocityTextures[2] = {0, 0};
GpuMesh gSheetMesh = {};
GLint gMVPUniform = -1;
GLint gTimeUniform = -1;
GLint gInteractionUniform = -1;
GLint gClothPositionRenderUniform = -1;
GLint gClothResolutionRenderUniform = -1;
GLint gLightDirectionUniform = -1;
GLint gCameraPositionUniform = -1;
GLint gMaterialBaseColorUniform = -1;
GLint gMaterialRoughnessUniform = -1;
GLint gMaterialSpecularStrengthUniform = -1;
GLint gMaterialReflectivityUniform = -1;
GLint gMaterialEffectsUniform = -1;
GLint gMaterialStructureUniform = -1;
GLint gSpectralWavelengthsUniform = -1;
GLint gOpticalEtaUniform = -1;
GLint gOpticalExtinctionUniform = -1;
GLint gEnvironmentLowColorUniform = -1;
GLint gEnvironmentHighColorUniform = -1;
GLint gEnvironmentHorizonColorUniform = -1;
GLint gEnvironmentSoftboxColorUniform = -1;
GLint gEnvironmentControlsUniform = -1;
GLint gQualityControlsUniform = -1;
GLint gDebugViewUniform = -1;
GLint gSpectralDebugIndexUniform = -1;
GLint gPolarizationModeUniform = -1;
GLint gSimPositionTexUniform = -1;
GLint gSimVelocityTexUniform = -1;
GLint gSimResolutionUniform = -1;
GLint gSimDeltaTimeUniform = -1;
GLint gSimTimeUniform = -1;
GLint gSimInteractionUniform = -1;
GLint gSimMaterialEffectsUniform = -1;

int gCanvasWidth = 800;
int gCanvasHeight = 600;
constexpr int kClothResolution = 64;

bool gDragging = false;
bool gSheetPulling = false;
double gLastMouseX = 0.0;
double gLastMouseY = 0.0;
float gCameraYaw = 0.65f;
float gCameraPitch = 0.55f;
float gCameraDistance = 3.4f;
float gInteractionX = 0.0f;
float gInteractionZ = 0.0f;
float gInteractionStrength = 0.0f;
float gInteractionRadius = 0.38f;
float gMotionSeed = 0.0f;
double gLastFrameTime = 0.0;
DirectionalLight gLight = createDefaultDirectionalLight();
Material gMaterial = createDefaultMetalMaterial();
Environment gEnvironment = createDefaultEnvironment();
int gDebugView = 0;
int gSpectralDebugIndex = 0;
int gLightPresetIndex = 0;
int gClothReadIndex = 0;
int gQualityMode = 1;
int gFrameCounter = 0;
int gPolarizationMode = 0;

const float kPi = 3.14159265358979323846f;

const char* qualityModeName(int mode) {
    if (mode == 0) {
        return "Fast";
    }
    if (mode == 2) {
        return "Quality";
    }
    return "Balanced";
}

const char* polarizationModeName(int mode) {
    if (mode == 1) {
        return "S";
    }
    if (mode == 2) {
        return "P";
    }
    return "Mixed";
}

void refreshOverlay() {
    updateOverlay(
        materialName(gMaterial.type),
        lightPresetName(gLightPresetIndex),
        qualityModeName(gQualityMode),
        polarizationModeName(gPolarizationMode),
        gDebugView,
        gMaterial.roughness,
        gMaterial.reflectivity,
        gMaterial.effectStrengths.x,
        gMaterial.effectStrengths.y,
        gMaterial.effectStrengths.z,
        gMaterial.structure.grooveSpacingNm,
        gMaterial.structure.grooveDepthNm,
        gMaterial.structure.layerThicknessNm,
        gMaterial.structure.disorderStrength,
        gInteractionStrength,
        gSpectralDebugIndex,
        gMaterial.optical.wavelengthsNm[gSpectralDebugIndex]
    );
}

void initSheetMesh() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    generateSheetMesh(kClothResolution - 1, kClothResolution - 1, 2.0f, 2.0f, vertices, indices);
    gSheetMesh = uploadIndexedPositionMesh(vertices, indices);
}

Vec3 initialClothPosition(int x, int y) {
    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kClothResolution);
    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kClothResolution);
    const float compressedV = 0.10f + v * 0.20f;
    return {
        (u - 0.5f) * 2.0f * 0.72f,
        0.86f - compressedV * 1.76f,
        -0.18f + std::sin(compressedV * kPi) * 0.50f
    };
}

GLuint createClothTexture(const std::vector<float>& data) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA32F,
        kClothResolution,
        kClothResolution,
        0,
        GL_RGBA,
        GL_FLOAT,
        data.data()
    );
    return texture;
}

void fillClothState(std::vector<float>& positions, std::vector<float>& velocities) {
    positions.assign(kClothResolution * kClothResolution * 4, 0.0f);
    velocities.assign(kClothResolution * kClothResolution * 4, 0.0f);

    for (int y = 0; y < kClothResolution; ++y) {
        for (int x = 0; x < kClothResolution; ++x) {
            const int index = (y * kClothResolution + x) * 4;
            const Vec3 position = initialClothPosition(x, y);
            positions[index + 0] = position.x;
            positions[index + 1] = position.y;
            positions[index + 2] = position.z;
            positions[index + 3] = 1.0f;
        }
    }
}

void resetClothSimulation() {
    std::vector<float> positions;
    std::vector<float> velocities;
    fillClothState(positions, velocities);

    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, gClothPositionTextures[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kClothResolution, kClothResolution, GL_RGBA, GL_FLOAT, positions.data());
        glBindTexture(GL_TEXTURE_2D, gClothVelocityTextures[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kClothResolution, kClothResolution, GL_RGBA, GL_FLOAT, velocities.data());
    }

    gMotionSeed = static_cast<float>(std::fmod(emscripten_get_now() * 0.001, 1000.0));
}

void initClothSimulation() {
    std::vector<float> positions;
    std::vector<float> velocities;
    fillClothState(positions, velocities);

    for (int i = 0; i < 2; ++i) {
        gClothPositionTextures[i] = createClothTexture(positions);
        gClothVelocityTextures[i] = createClothTexture(velocities);
    }

    glGenFramebuffers(1, &gClothFramebuffer);
    glGenVertexArrays(1, &gClothSimVao);
    gMotionSeed = static_cast<float>(std::fmod(emscripten_get_now() * 0.001, 1000.0));
}

void simulateCloth(float dt, float timeSeconds) {
    const int writeIndex = 1 - gClothReadIndex;
    glUseProgram(gClothSimProgram);
    glViewport(0, 0, kClothResolution, kClothResolution);

    glBindFramebuffer(GL_FRAMEBUFFER, gClothFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gClothPositionTextures[writeIndex], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gClothVelocityTextures[writeIndex], 0);
    const GLenum attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gClothPositionTextures[gClothReadIndex]);
    glUniform1i(gSimPositionTexUniform, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gClothVelocityTextures[gClothReadIndex]);
    glUniform1i(gSimVelocityTexUniform, 1);

    glUniform2f(gSimResolutionUniform, static_cast<float>(kClothResolution), static_cast<float>(kClothResolution));
    glUniform1f(gSimDeltaTimeUniform, dt);
    glUniform1f(gSimTimeUniform, timeSeconds + gMotionSeed);
    glUniform4f(gSimInteractionUniform, gInteractionX, gInteractionZ, gInteractionStrength, gInteractionRadius);
    glUniform3f(gSimMaterialEffectsUniform, gMaterial.effectStrengths.x, gMaterial.effectStrengths.y, gMaterial.effectStrengths.z);

    glBindVertexArray(gClothSimVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gClothReadIndex = writeIndex;
}

void setMaterial(MaterialType type) {
    gMaterial = createMaterial(type);
    const char* name = materialName(type);

    if (type == MaterialType::AluminumFoil) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Aluminum Foil';");
    } else if (type == MaterialType::CoatedPlastic) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Coated Plastic';");
    } else if (type == MaterialType::CoatedMetal) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Coated Metal';");
    } else if (type == MaterialType::Copper) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Copper';");
    } else if (type == MaterialType::Gold) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Gold';");
    } else if (type == MaterialType::Silver) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Silver';");
    }

    (void)name;
    refreshOverlay();
}

void setDebugView(int debugView) {
    gDebugView = debugView;

    if (debugView == 0) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Shaded';");
    } else if (debugView == 1) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Groove Spacing';");
    } else if (debugView == 2) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Groove Depth';");
    } else if (debugView == 3) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Layer Thickness';");
    } else if (debugView == 4) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Disorder Roughness';");
    } else if (debugView == 5) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Height';");
    } else if (debugView == 6) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Normals Grooves';");
    } else if (debugView == 7) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Environment Reflection';");
    } else if (debugView == 8) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Reflection Direction';");
    } else if (debugView == 9) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Spectral Color';");
    } else if (debugView == 10) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Optical Constants';");
    } else if (debugView == 11) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Single Wavelength';");
    } else if (debugView == 12) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Angle Reflectance';");
    } else if (debugView == 13) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Tangent Frame';");
    } else if (debugView == 14) {
        emscripten_run_script("document.title = 'Spectral Sheet Renderer - Debug Diffraction';");
    }

    refreshOverlay();
}

void setLightPreset(int presetIndex) {
    gLightPresetIndex = (presetIndex % kLightPresetCount + kLightPresetCount) % kLightPresetCount;
    gLight = createDirectionalLightPreset(gLightPresetIndex);
    refreshOverlay();
}

void setQualityMode(int qualityMode) {
    gQualityMode = (qualityMode % 3 + 3) % 3;
    refreshOverlay();
}

void setPolarizationMode(int polarizationMode) {
    gPolarizationMode = (polarizationMode % 3 + 3) % 3;
    refreshOverlay();
}

void updateCanvasSize() {
    double cssWidth = 0.0;
    double cssHeight = 0.0;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);

    const double pixelRatio = emscripten_get_device_pixel_ratio();
    const int width = std::max(1, static_cast<int>(cssWidth * pixelRatio));
    const int height = std::max(1, static_cast<int>(cssHeight * pixelRatio));

    if (width != gCanvasWidth || height != gCanvasHeight) {
        gCanvasWidth = width;
        gCanvasHeight = height;
        emscripten_set_canvas_element_size("#canvas", gCanvasWidth, gCanvasHeight);
        glViewport(0, 0, gCanvasWidth, gCanvasHeight);
    }
}

Vec3 cameraPosition() {
    const float cosPitch = std::cos(gCameraPitch);
    return {
        gCameraDistance * cosPitch * std::sin(gCameraYaw),
        gCameraDistance * std::sin(gCameraPitch),
        gCameraDistance * cosPitch * std::cos(gCameraYaw)
    };
}

void updateSheetInteractionFromMouse(double clientX, double clientY) {
    double cssWidth = 1.0;
    double cssHeight = 1.0;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);

    const float u = std::clamp(static_cast<float>(clientX / std::max(cssWidth, 1.0)), 0.0f, 1.0f);
    const float v = std::clamp(static_cast<float>(clientY / std::max(cssHeight, 1.0)), 0.0f, 1.0f);
    gInteractionX = (u - 0.5f) * 2.0f;
    gInteractionZ = (v - 0.5f) * 2.0f;
    gInteractionStrength = 1.0f;
}

EM_BOOL onMouseDown(int, const EmscriptenMouseEvent* event, void*) {
    if (event->button == 0) {
        emscripten_run_script("Module.canvas.focus();");
        if (event->shiftKey) {
            gSheetPulling = true;
            gDragging = false;
            updateSheetInteractionFromMouse(event->clientX, event->clientY);
            refreshOverlay();
            return EM_TRUE;
        }

        gDragging = true;
        gLastMouseX = event->clientX;
        gLastMouseY = event->clientY;
        return EM_TRUE;
    }
    return EM_FALSE;
}

EM_BOOL onMouseUp(int, const EmscriptenMouseEvent*, void*) {
    gDragging = false;
    gSheetPulling = false;
    return EM_TRUE;
}

EM_BOOL onMouseMove(int, const EmscriptenMouseEvent* event, void*) {
    if (gSheetPulling) {
        updateSheetInteractionFromMouse(event->clientX, event->clientY);
        refreshOverlay();
        return EM_TRUE;
    }

    if (!gDragging) {
        return EM_FALSE;
    }

    const double dx = event->clientX - gLastMouseX;
    const double dy = event->clientY - gLastMouseY;
    gLastMouseX = event->clientX;
    gLastMouseY = event->clientY;

    gCameraYaw += static_cast<float>(dx) * 0.008f;
    gCameraPitch += static_cast<float>(dy) * 0.008f;
    gCameraPitch = std::clamp(gCameraPitch, -1.45f, 1.45f);
    return EM_TRUE;
}

EM_BOOL onWheel(int, const EmscriptenWheelEvent* event, void*) {
    gCameraDistance += static_cast<float>(event->deltaY) * 0.002f;
    gCameraDistance = std::clamp(gCameraDistance, 1.25f, 8.0f);
    return EM_TRUE;
}

EM_BOOL onKeyDown(int, const EmscriptenKeyboardEvent* event, void*) {
    const char key = event->key[0];

    if (event->keyCode == 49 || key == '1') {
        setMaterial(MaterialType::AluminumFoil);
        return EM_TRUE;
    }

    if (event->keyCode == 50 || key == '2') {
        setMaterial(MaterialType::CoatedPlastic);
        return EM_TRUE;
    }

    if (event->keyCode == 51 || key == '3') {
        setMaterial(MaterialType::CoatedMetal);
        return EM_TRUE;
    }

    if (event->keyCode == 52 || key == '4') {
        setMaterial(MaterialType::Copper);
        return EM_TRUE;
    }

    if (event->keyCode == 53 || key == '5') {
        setMaterial(MaterialType::Gold);
        return EM_TRUE;
    }

    if (event->keyCode == 54 || key == '6') {
        setMaterial(MaterialType::Silver);
        return EM_TRUE;
    }

    if (event->keyCode == 48 || key == '0') {
        setDebugView(0);
        return EM_TRUE;
    }

    if (event->keyCode == 55 || key == '7') {
        setDebugView(1);
        return EM_TRUE;
    }

    if (event->keyCode == 56 || key == '8') {
        setDebugView(2);
        return EM_TRUE;
    }

    if (event->keyCode == 57 || key == '9') {
        setDebugView(3);
        return EM_TRUE;
    }

    if (key == 'q' || key == 'Q') {
        setDebugView(4);
        return EM_TRUE;
    }

    if (key == 'w' || key == 'W') {
        setDebugView(5);
        return EM_TRUE;
    }

    if (key == 'e' || key == 'E') {
        setDebugView(6);
        return EM_TRUE;
    }

    if (key == 'r' || key == 'R') {
        setDebugView(7);
        return EM_TRUE;
    }

    if (key == 't' || key == 'T') {
        setDebugView(8);
        return EM_TRUE;
    }

    if (key == 'y' || key == 'Y') {
        setDebugView(9);
        return EM_TRUE;
    }

    if (key == 'u' || key == 'U') {
        setDebugView(10);
        return EM_TRUE;
    }

    if (key == 'i' || key == 'I') {
        gSpectralDebugIndex = (gSpectralDebugIndex + 1) % kSpectralSampleCount;
        setDebugView(11);
        return EM_TRUE;
    }

    if (key == 'o' || key == 'O') {
        setDebugView(12);
        return EM_TRUE;
    }

    if (key == 'p' || key == 'P') {
        setDebugView(13);
        return EM_TRUE;
    }

    if (key == 'd' || key == 'D') {
        setDebugView(14);
        return EM_TRUE;
    }

    if (key == 'l' || key == 'L') {
        setLightPreset(gLightPresetIndex + 1);
        return EM_TRUE;
    }

    if (key == 'k' || key == 'K') {
        setQualityMode(gQualityMode + 1);
        return EM_TRUE;
    }

    if (key == 'z' || key == 'Z') {
        setPolarizationMode(gPolarizationMode + 1);
        return EM_TRUE;
    }

    if (key == 'f' || key == 'F') {
        resetClothSimulation();
        refreshOverlay();
        return EM_TRUE;
    }

    return EM_FALSE;
}

void render() {
    updateCanvasSize();
    const double nowSeconds = emscripten_get_now() * 0.001;
    float dt = 1.0f / 60.0f;
    if (gLastFrameTime > 0.0) {
        dt = std::clamp(static_cast<float>(nowSeconds - gLastFrameTime), 0.0f, 0.033f);
    }
    gLastFrameTime = nowSeconds;

    if (!gSheetPulling) {
        gInteractionStrength *= 0.965f;
        if (gInteractionStrength < 0.001f) {
            gInteractionStrength = 0.0f;
        }
    }
    ++gFrameCounter;
    const bool skipFastFrame = gQualityMode == 0 && !gSheetPulling && (gFrameCounter % 2) != 0;
    if (!skipFastFrame) {
        simulateCloth(gQualityMode == 0 ? dt * 2.0f : dt, static_cast<float>(nowSeconds));
    }
    glViewport(0, 0, gCanvasWidth, gCanvasHeight);

    glClearColor(0.06f, 0.075f, 0.085f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = static_cast<float>(gCanvasWidth) / static_cast<float>(gCanvasHeight);
    const Mat4 projection = perspective(50.0f * kPi / 180.0f, aspect, 0.01f, 100.0f);
    const Vec3 cameraPos = cameraPosition();
    const Mat4 view = lookAt(cameraPos, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    const Mat4 model = identity();
    const Mat4 mvp = multiply(projection, multiply(view, model));

    glUseProgram(gProgram);
    glUniformMatrix4fv(gMVPUniform, 1, GL_FALSE, mvp.m);
    glUniform1f(gTimeUniform, static_cast<float>(nowSeconds));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gClothPositionTextures[gClothReadIndex]);
    glUniform1i(gClothPositionRenderUniform, 0);
    glUniform2f(gClothResolutionRenderUniform, static_cast<float>(kClothResolution), static_cast<float>(kClothResolution));
    glUniform3f(gCameraPositionUniform, cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform1i(gDebugViewUniform, gDebugView);
    glUniform1i(gSpectralDebugIndexUniform, gSpectralDebugIndex);
    glUniform1i(gPolarizationModeUniform, gPolarizationMode);
    const float maxDiffractionOrder = gQualityMode == 0 ? 1.0f : 2.0f;
    const float wavelengthStep = gQualityMode == 0 ? 2.0f : 1.0f;
    glUniform4f(gQualityControlsUniform, maxDiffractionOrder, wavelengthStep, static_cast<float>(gQualityMode), 0.0f);
    uploadDirectionalLight(gLightDirectionUniform, gLight);
    uploadEnvironment(
        gEnvironmentLowColorUniform,
        gEnvironmentHighColorUniform,
        gEnvironmentHorizonColorUniform,
        gEnvironmentSoftboxColorUniform,
        gEnvironmentControlsUniform,
        gEnvironment
    );
    uploadMaterial(
        gMaterialBaseColorUniform,
        gMaterialRoughnessUniform,
        gMaterialSpecularStrengthUniform,
        gMaterialReflectivityUniform,
        gMaterialEffectsUniform,
        gMaterialStructureUniform,
        gSpectralWavelengthsUniform,
        gOpticalEtaUniform,
        gOpticalExtinctionUniform,
        gMaterial
    );
    drawGpuMesh(gSheetMesh);
}

int main() {
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.majorVersion = 2;
    attr.minorVersion = 0;
    attr.depth = EM_TRUE;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = emscripten_webgl_create_context("#canvas", &attr);
    emscripten_webgl_make_context_current(context);
    emscripten_webgl_enable_extension(context, "EXT_color_buffer_float");

    updateCanvasSize();
    glEnable(GL_DEPTH_TEST);

    gProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    gClothSimProgram = createShaderProgram(clothSimVertexShaderSource, clothSimFragmentShaderSource);
    gMVPUniform = glGetUniformLocation(gProgram, "uMVP");
    gTimeUniform = glGetUniformLocation(gProgram, "uTime");
    gInteractionUniform = glGetUniformLocation(gProgram, "uInteraction");
    gClothPositionRenderUniform = glGetUniformLocation(gProgram, "uClothPositionTex");
    gClothResolutionRenderUniform = glGetUniformLocation(gProgram, "uClothResolution");
    gLightDirectionUniform = glGetUniformLocation(gProgram, "uLightDir");
    gCameraPositionUniform = glGetUniformLocation(gProgram, "uCameraPos");
    gMaterialBaseColorUniform = glGetUniformLocation(gProgram, "uMaterialBaseColor");
    gMaterialRoughnessUniform = glGetUniformLocation(gProgram, "uMaterialRoughness");
    gMaterialSpecularStrengthUniform = glGetUniformLocation(gProgram, "uMaterialSpecularStrength");
    gMaterialReflectivityUniform = glGetUniformLocation(gProgram, "uMaterialReflectivity");
    gMaterialEffectsUniform = glGetUniformLocation(gProgram, "uMaterialEffects");
    gMaterialStructureUniform = glGetUniformLocation(gProgram, "uMaterialStructure");
    gSpectralWavelengthsUniform = glGetUniformLocation(gProgram, "uSpectralWavelengths[0]");
    gOpticalEtaUniform = glGetUniformLocation(gProgram, "uOpticalEta[0]");
    gOpticalExtinctionUniform = glGetUniformLocation(gProgram, "uOpticalExtinction[0]");
    gEnvironmentLowColorUniform = glGetUniformLocation(gProgram, "uEnvLowColor");
    gEnvironmentHighColorUniform = glGetUniformLocation(gProgram, "uEnvHighColor");
    gEnvironmentHorizonColorUniform = glGetUniformLocation(gProgram, "uEnvHorizonColor");
    gEnvironmentSoftboxColorUniform = glGetUniformLocation(gProgram, "uEnvSoftboxColor");
    gEnvironmentControlsUniform = glGetUniformLocation(gProgram, "uEnvControls");
    gQualityControlsUniform = glGetUniformLocation(gProgram, "uQualityControls");
    gDebugViewUniform = glGetUniformLocation(gProgram, "uDebugView");
    gSpectralDebugIndexUniform = glGetUniformLocation(gProgram, "uSpectralDebugIndex");
    gPolarizationModeUniform = glGetUniformLocation(gProgram, "uPolarizationMode");
    gSimPositionTexUniform = glGetUniformLocation(gClothSimProgram, "uPositionTex");
    gSimVelocityTexUniform = glGetUniformLocation(gClothSimProgram, "uVelocityTex");
    gSimResolutionUniform = glGetUniformLocation(gClothSimProgram, "uResolution");
    gSimDeltaTimeUniform = glGetUniformLocation(gClothSimProgram, "uDeltaTime");
    gSimTimeUniform = glGetUniformLocation(gClothSimProgram, "uTime");
    gSimInteractionUniform = glGetUniformLocation(gClothSimProgram, "uInteraction");
    gSimMaterialEffectsUniform = glGetUniformLocation(gClothSimProgram, "uMaterialEffects");
    initSheetMesh();
    initClothSimulation();

    emscripten_set_mousedown_callback("#canvas", nullptr, EM_TRUE, onMouseDown);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onMouseUp);
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onMouseMove);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_TRUE, onWheel);
    emscripten_set_keydown_callback("#canvas", nullptr, EM_TRUE, onKeyDown);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_TRUE, onKeyDown);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onKeyDown);
    setMaterial(MaterialType::CoatedMetal);
    setLightPreset(0);

    emscripten_set_main_loop(render, 0, 1);
    return 0;
}
