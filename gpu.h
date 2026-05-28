#pragma once

#include <vector>

#include <GLES3/gl3.h>

struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
};

GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource);
GpuMesh uploadIndexedPositionMesh(
    const std::vector<float>& vertices,
    const std::vector<unsigned int>& indices
);
void drawGpuMesh(const GpuMesh& mesh);
