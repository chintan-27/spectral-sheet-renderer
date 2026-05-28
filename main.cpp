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
    int debugView,
    float roughness,
    float reflectivity,
    float grooveSpacingNm,
    float grooveDepthNm,
    float layerThicknessNm,
    float disorderStrength,
    int spectralIndex,
    float spectralWavelengthNm
), {
    const overlay = document.getElementById('debug-overlay');
    if (!overlay) {
        return;
    }

    const materialName = UTF8ToString(materialNamePtr);
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

    overlay.innerHTML =
        '<div class="title">Material Debug</div>' +
        '<div>Material: ' + materialName + '</div>' +
        '<div>View: ' + debugName + '</div>' +
        '<br>' +
        '<div>Roughness: ' + roughness.toFixed(3) + '</div>' +
        '<div>Reflectivity: ' + reflectivity.toFixed(3) + '</div>' +
        '<div>Groove spacing: ' + grooveSpacingNm.toFixed(1) + ' nm</div>' +
        '<div>Groove depth: ' + grooveDepthNm.toFixed(1) + ' nm</div>' +
        '<div>Layer thickness: ' + layerThicknessNm.toFixed(1) + ' nm</div>' +
        '<div>Disorder: ' + disorderStrength.toFixed(3) + '</div>' +
        '<div>Spectral sample: #' + spectralIndex + ' / ' + spectralWavelengthNm.toFixed(1) + ' nm</div>' +
        '<br>' +
        '<div class="muted">Materials: 1 Al, 2 plastic, 3 coated, 4 Cu, 5 Au, 6 Ag</div>' +
        '<div class="muted">Views: 0 shaded, 7 spacing, 8 depth, 9 layer, Q disorder, W height, E normals, R env, T reflection, Y spectral, U n/k, I wavelength, O angle, P frame, D diffraction</div>';
});

GLuint gProgram = 0;
GpuMesh gSheetMesh = {};
GLint gMVPUniform = -1;
GLint gTimeUniform = -1;
GLint gLightDirectionUniform = -1;
GLint gCameraPositionUniform = -1;
GLint gMaterialBaseColorUniform = -1;
GLint gMaterialRoughnessUniform = -1;
GLint gMaterialSpecularStrengthUniform = -1;
GLint gMaterialReflectivityUniform = -1;
GLint gMaterialStructureUniform = -1;
GLint gSpectralWavelengthsUniform = -1;
GLint gOpticalEtaUniform = -1;
GLint gOpticalExtinctionUniform = -1;
GLint gEnvironmentLowColorUniform = -1;
GLint gEnvironmentHighColorUniform = -1;
GLint gEnvironmentHorizonColorUniform = -1;
GLint gEnvironmentSoftboxColorUniform = -1;
GLint gEnvironmentControlsUniform = -1;
GLint gDebugViewUniform = -1;
GLint gSpectralDebugIndexUniform = -1;

int gCanvasWidth = 800;
int gCanvasHeight = 600;

bool gDragging = false;
double gLastMouseX = 0.0;
double gLastMouseY = 0.0;
float gCameraYaw = 0.65f;
float gCameraPitch = 0.55f;
float gCameraDistance = 3.4f;
DirectionalLight gLight = createDefaultDirectionalLight();
Material gMaterial = createDefaultMetalMaterial();
Environment gEnvironment = createDefaultEnvironment();
int gDebugView = 0;
int gSpectralDebugIndex = 0;

const float kPi = 3.14159265358979323846f;

void refreshOverlay() {
    updateOverlay(
        materialName(gMaterial.type),
        gDebugView,
        gMaterial.roughness,
        gMaterial.reflectivity,
        gMaterial.structure.grooveSpacingNm,
        gMaterial.structure.grooveDepthNm,
        gMaterial.structure.layerThicknessNm,
        gMaterial.structure.disorderStrength,
        gSpectralDebugIndex,
        gMaterial.optical.wavelengthsNm[gSpectralDebugIndex]
    );
}

void initSheetMesh() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    generateSheetMesh(48, 48, 2.0f, 2.0f, vertices, indices);
    gSheetMesh = uploadIndexedPositionMesh(vertices, indices);
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

EM_BOOL onMouseDown(int, const EmscriptenMouseEvent* event, void*) {
    if (event->button == 0) {
        emscripten_run_script("Module.canvas.focus();");
        gDragging = true;
        gLastMouseX = event->clientX;
        gLastMouseY = event->clientY;
        return EM_TRUE;
    }
    return EM_FALSE;
}

EM_BOOL onMouseUp(int, const EmscriptenMouseEvent*, void*) {
    gDragging = false;
    return EM_TRUE;
}

EM_BOOL onMouseMove(int, const EmscriptenMouseEvent* event, void*) {
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

    return EM_FALSE;
}

void render() {
    updateCanvasSize();

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
    glUniform1f(gTimeUniform, static_cast<float>(emscripten_get_now() * 0.001));
    glUniform3f(gCameraPositionUniform, cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform1i(gDebugViewUniform, gDebugView);
    glUniform1i(gSpectralDebugIndexUniform, gSpectralDebugIndex);
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

    updateCanvasSize();
    glEnable(GL_DEPTH_TEST);

    gProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    gMVPUniform = glGetUniformLocation(gProgram, "uMVP");
    gTimeUniform = glGetUniformLocation(gProgram, "uTime");
    gLightDirectionUniform = glGetUniformLocation(gProgram, "uLightDir");
    gCameraPositionUniform = glGetUniformLocation(gProgram, "uCameraPos");
    gMaterialBaseColorUniform = glGetUniformLocation(gProgram, "uMaterialBaseColor");
    gMaterialRoughnessUniform = glGetUniformLocation(gProgram, "uMaterialRoughness");
    gMaterialSpecularStrengthUniform = glGetUniformLocation(gProgram, "uMaterialSpecularStrength");
    gMaterialReflectivityUniform = glGetUniformLocation(gProgram, "uMaterialReflectivity");
    gMaterialStructureUniform = glGetUniformLocation(gProgram, "uMaterialStructure");
    gSpectralWavelengthsUniform = glGetUniformLocation(gProgram, "uSpectralWavelengths[0]");
    gOpticalEtaUniform = glGetUniformLocation(gProgram, "uOpticalEta[0]");
    gOpticalExtinctionUniform = glGetUniformLocation(gProgram, "uOpticalExtinction[0]");
    gEnvironmentLowColorUniform = glGetUniformLocation(gProgram, "uEnvLowColor");
    gEnvironmentHighColorUniform = glGetUniformLocation(gProgram, "uEnvHighColor");
    gEnvironmentHorizonColorUniform = glGetUniformLocation(gProgram, "uEnvHorizonColor");
    gEnvironmentSoftboxColorUniform = glGetUniformLocation(gProgram, "uEnvSoftboxColor");
    gEnvironmentControlsUniform = glGetUniformLocation(gProgram, "uEnvControls");
    gDebugViewUniform = glGetUniformLocation(gProgram, "uDebugView");
    gSpectralDebugIndexUniform = glGetUniformLocation(gProgram, "uSpectralDebugIndex");
    initSheetMesh();

    emscripten_set_mousedown_callback("#canvas", nullptr, EM_TRUE, onMouseDown);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onMouseUp);
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onMouseMove);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_TRUE, onWheel);
    emscripten_set_keydown_callback("#canvas", nullptr, EM_TRUE, onKeyDown);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_TRUE, onKeyDown);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onKeyDown);
    setMaterial(MaterialType::AluminumFoil);

    emscripten_set_main_loop(render, 0, 1);
    return 0;
}
