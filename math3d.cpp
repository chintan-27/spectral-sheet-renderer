#include "math3d.h"

#include <cmath>

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
