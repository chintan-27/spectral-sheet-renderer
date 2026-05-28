#pragma once

#include <vector>

void generateSheetMesh(
    int rows,
    int cols,
    float width,
    float depth,
    std::vector<float>& vertices,
    std::vector<unsigned int>& indices
);
