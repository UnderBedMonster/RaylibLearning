#pragma once
#include <iostream>
#include <cmath>
#include <raymath.h>


class QuaternionR {
public:
    float w, x, y, z;

    // Constructor
    QuaternionR(float w = 1.0f, float x = 0.0f, float y = 0.0f, float z = 0.0f)
        : w(w), x(x), y(y), z(z) {}

    QuaternionR(float angle, Vector3 v) {
        float halfAngle = angle * 0.5f;
        float sinHalfAngle = std::sin(halfAngle);

        w = std::cos(halfAngle);
        x = v.x * sinHalfAngle;
        y = v.y * sinHalfAngle;
        z = v.z * sinHalfAngle;
        normalize();
    }

    // Normalize the quaternion
    void normalize() {
        float norm = std::sqrt(w * w + x * x + y * y + z * z);
        w /= norm;
        x /= norm;
        y /= norm;
        z /= norm;
    }

    // Conjugate of the quaternion
    QuaternionR conjugate() const {
        return QuaternionR(w, -x, -y, -z);
    }

    // Multiply two quaternions
    QuaternionR operator*(const QuaternionR& q) const {
        return QuaternionR(
            w * q.w - x * q.x - y * q.y - z * q.z,
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w
        );
    }

    Matrix toMatrix() const {
        return Matrix{
            1 - 2 * (y * y + z * z),  2 * (x * y - w * z),      2 * (x * z + w * y),      0,
            2 * (x * y + w * z),      1 - 2 * (x * x + z * z),  2 * (y * z - w * x),      0,
            2 * (x * z - w * y),      2 * (y * z + w * x),      1 - 2 * (x * x + y * y),  0,
            0,                  0,                  0,                  1
        };
    }

    // Rotate a vector using the quaternion
    void rotateVector(Vector3& v) const {
        QuaternionR p(0, v.x, v.y, v.z);
        QuaternionR q_conj = conjugate();
        QuaternionR rotated = (*this) * p * q_conj;

        v.x = rotated.x;
        v.y = rotated.y;
        v.z = rotated.z;
    }
};