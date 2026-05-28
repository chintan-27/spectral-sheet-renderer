#include <algorithm>
#include <cmath>
#include <vector>

#include <GLES3/gl3.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "gpu.h"
#include "light.h"
#include "math3d.h"
#include "shaders.h"
#include "sheet_mesh.h"

GLuint gProgram = 0;
GpuMesh gSheetMesh = {};
GLint gMVPUniform = -1;
GLint gTimeUniform = -1;
GLint gLightDirectionUniform = -1;

int gCanvasWidth = 800;
int gCanvasHeight = 600;

bool gDragging = false;
double gLastMouseX = 0.0;
double gLastMouseY = 0.0;
float gCameraYaw = 0.65f;
float gCameraPitch = 0.55f;
float gCameraDistance = 3.4f;
DirectionalLight gLight = createDefaultDirectionalLight();

const float kPi = 3.14159265358979323846f;

void initSheetMesh() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    generateSheetMesh(48, 48, 2.0f, 2.0f, vertices, indices);
    gSheetMesh = uploadIndexedPositionMesh(vertices, indices);
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

void render() {
    updateCanvasSize();

    glClearColor(0.06f, 0.075f, 0.085f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = static_cast<float>(gCanvasWidth) / static_cast<float>(gCanvasHeight);
    const Mat4 projection = perspective(50.0f * kPi / 180.0f, aspect, 0.01f, 100.0f);
    const Mat4 view = lookAt(cameraPosition(), {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    const Mat4 model = identity();
    const Mat4 mvp = multiply(projection, multiply(view, model));

    glUseProgram(gProgram);
    glUniformMatrix4fv(gMVPUniform, 1, GL_FALSE, mvp.m);
    glUniform1f(gTimeUniform, static_cast<float>(emscripten_get_now() * 0.001));
    uploadDirectionalLight(gLightDirectionUniform, gLight);
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
    initSheetMesh();

    emscripten_set_mousedown_callback("#canvas", nullptr, EM_TRUE, onMouseDown);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onMouseUp);
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onMouseMove);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_TRUE, onWheel);

    emscripten_set_main_loop(render, 0, 1);
    return 0;
}
