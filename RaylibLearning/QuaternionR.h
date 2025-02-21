#pragma once
#include <cmath>
#include <raymath.h>

class QuaternionR : public Quaternion
{

public:
    Vector4 q{0};

    QuaternionR(float w, float x, float y, float z) {
    
        q.w = w;
        q.x = x;
        q.y = y;
        q.z = z;
    }

    QuaternionR(const Vector3& axis, float angle) {
        float halfAngle = angle / 2.0;
        float sinHalfAngle = std::sin(halfAngle);
         
        q.w = std::cos(halfAngle);
        q.x = axis.x* sinHalfAngle;
        q.y = axis.y* sinHalfAngle;
        q.z = axis.z* sinHalfAngle;
        
    }

    ~QuaternionR() { }

    QuaternionR operator*(const QuaternionR& q) const {
        return QuaternionR{
            w * q.w - x * q.x - y * q.y - z * q.z,
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w
        };
    }

    // Normalize the quaternion
    void normalize() {
        double mag = std::sqrt(w * w + x * x + y * y + z * z);
        w /= mag;
        x /= mag;
        y /= mag;
        z /= mag;
    }

    

    // Quaternion conjugate
    QuaternionR conjugate() const {
        return QuaternionR{ w, -x, -y, -z };
    }
};

