#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include <GLES3/gl3.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Mat4 {
    float m[16];
};

GLuint gProgram = 0;
GLuint gVBO = 0;
GLuint gEBO = 0;
GLuint gVAO = 0;
GLint gMVPUniform = -1;
GLsizei gIndexCount = 0;

int gCanvasWidth = 800;
int gCanvasHeight = 600;

bool gDragging = false;
double gLastMouseX = 0.0;
double gLastMouseY = 0.0;
float gCameraYaw = 0.65f;
float gCameraPitch = 0.55f;
float gCameraDistance = 3.4f;

const float kPi = 3.14159265358979323846f;

const char* vertexShaderSource = R"(#version 300 es
layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;

out vec3 vWorldPos;

void main() {
    vWorldPos = aPos;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(#version 300 es
precision mediump float;

in vec3 vWorldPos;
out vec4 FragColor;

void main() {
    vec2 cell = abs(fract(vWorldPos.xz * 24.0) - 0.5);
    float line = 1.0 - smoothstep(0.465, 0.5, min(cell.x, cell.y));
    vec3 baseColor = vec3(0.42, 0.55, 0.63);
    vec3 gridColor = vec3(0.08, 0.12, 0.15);
    FragColor = vec4(mix(baseColor, gridColor, line * 0.55), 1.0);
}
)";

Vec3 subtract(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 normalize(Vec3 v) {
    const float length = std::sqrt(dot(v, v));
    if (length <= 0.000001f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return {v.x / length, v.y / length, v.z / length};
}

Mat4 identity() {
    Mat4 result = {};
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

Mat4 multiply(Mat4 a, Mat4 b) {
    Mat4 result = {};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            result.m[col * 4 + row] = sum;
        }
    }
    return result;
}

Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane) {
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 result = {};
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return result;
}

Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 f = normalize(subtract(target, eye));
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 result = identity();
    result.m[0] = s.x;
    result.m[4] = s.y;
    result.m[8] = s.z;
    result.m[1] = u.x;
    result.m[5] = u.y;
    result.m[9] = u.z;
    result.m[2] = -f.x;
    result.m[6] = -f.y;
    result.m[10] = -f.z;
    result.m[12] = -dot(s, eye);
    result.m[13] = -dot(u, eye);
    result.m[14] = dot(f, eye);
    return result;
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void generateSheetMesh(
    int rows,
    int cols,
    float width,
    float depth,
    std::vector<float>& vertices,
    std::vector<unsigned int>& indices
) {
    vertices.clear();
    indices.clear();
    vertices.reserve(static_cast<size_t>((rows + 1) * (cols + 1) * 3));
    indices.reserve(static_cast<size_t>(rows * cols * 6));

    for (int row = 0; row <= rows; ++row) {
        const float v = static_cast<float>(row) / static_cast<float>(rows);
        for (int col = 0; col <= cols; ++col) {
            const float u = static_cast<float>(col) / static_cast<float>(cols);
            vertices.push_back(width * (u - 0.5f));
            vertices.push_back(0.0f);
            vertices.push_back(depth * (v - 0.5f));
        }
    }

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const unsigned int topLeft = static_cast<unsigned int>(row * (cols + 1) + col);
            const unsigned int topRight = topLeft + 1;
            const unsigned int bottomLeft = static_cast<unsigned int>((row + 1) * (cols + 1) + col);
            const unsigned int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
}

void initSheetMesh() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    generateSheetMesh(48, 48, 2.0f, 2.0f, vertices, indices);
    gIndexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &gVAO);
    glGenBuffers(1, &gVBO);
    glGenBuffers(1, &gEBO);

    glBindVertexArray(gVAO);

    glBindBuffer(GL_ARRAY_BUFFER, gVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gEBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
        indices.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
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
    glBindVertexArray(gVAO);
    glDrawElements(GL_TRIANGLES, gIndexCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(0));
    glBindVertexArray(0);
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

    gProgram = createProgram(vertexShaderSource, fragmentShaderSource);
    gMVPUniform = glGetUniformLocation(gProgram, "uMVP");
    initSheetMesh();

    emscripten_set_mousedown_callback("#canvas", nullptr, EM_TRUE, onMouseDown);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onMouseUp);
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onMouseMove);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_TRUE, onWheel);

    emscripten_set_main_loop(render, 0, 1);
    return 0;
}
