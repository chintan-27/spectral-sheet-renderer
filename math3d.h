#pragma once

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Mat4 {
    float m[16];
};

Vec3 subtract(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
float dot(Vec3 a, Vec3 b);
Vec3 normalize(Vec3 v);

Mat4 identity();
Mat4 multiply(Mat4 a, Mat4 b);
Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane);
Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up);
