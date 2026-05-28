#include "sheet_mesh.h"

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
