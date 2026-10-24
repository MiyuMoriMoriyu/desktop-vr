#pragma once

#include <cmath>

#include "openvr_driver.h"

constexpr double kPi = 3.14159265358979323846;

inline double Rad(double degrees)
{
    return degrees * kPi / 180.0;
}

struct Vec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 operator*(const Vec3& a, double s) { return { a.x * s, a.y * s, a.z * s }; }

inline Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline Vec3 Lerp(const Vec3& a, const Vec3& b, double t)
{
    return a + (b - a) * t;
}

inline Vec3 Normalize(const Vec3& v)
{
    const double length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return length > 0.0 ? v * (1.0 / length) : v;
}

using Quat = vr::HmdQuaternion_t;

inline Quat operator*(const Quat& a, const Quat& b)
{
    return {
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

inline Quat Conjugate(const Quat& q)
{
    return { q.w, -q.x, -q.y, -q.z };
}

inline Vec3 Rotate(const Quat& q, const Vec3& v)
{
    const Quat r = q * Quat{ 0.0, v.x, v.y, v.z } * Conjugate(q);
    return { r.x, r.y, r.z };
}

inline Quat AxisAngle(const Vec3& axis, double angle)
{
    const double s = std::sin(angle * 0.5);
    return { std::cos(angle * 0.5), axis.x * s, axis.y * s, axis.z * s };
}

inline Quat Normalize(const Quat& q)
{
    const double length = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return { q.w / length, q.x / length, q.y / length, q.z / length };
}

inline Quat Slerp(const Quat& a, Quat b, double t)
{
    double cosine = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (cosine < 0.0)
    {
        b = { -b.w, -b.x, -b.y, -b.z };
        cosine = -cosine;
    }
    double wa = 1.0 - t;
    double wb = t;
    if (cosine < 0.9995)
    {
        const double angle = std::acos(cosine);
        const double sine = std::sin(angle);
        wa = std::sin((1.0 - t) * angle) / sine;
        wb = std::sin(t * angle) / sine;
    }
    return Normalize(Quat{ a.w * wa + b.w * wb, a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb });
}

inline Quat LookRotation(const Vec3& forward, const Vec3& upHint)
{
    const Vec3 back = Normalize(forward * -1.0);
    const Vec3 right = Normalize(Cross(upHint, back));
    const Vec3 up = Cross(back, right);

    const double trace = right.x + up.y + back.z;
    if (trace > 0.0)
    {
        const double s = std::sqrt(trace + 1.0) * 2.0;
        return { 0.25 * s, (up.z - back.y) / s, (back.x - right.z) / s, (right.y - up.x) / s };
    }
    if (right.x > up.y && right.x > back.z)
    {
        const double s = std::sqrt(1.0 + right.x - up.y - back.z) * 2.0;
        return { (up.z - back.y) / s, 0.25 * s, (up.x + right.y) / s, (back.x + right.z) / s };
    }
    if (up.y > back.z)
    {
        const double s = std::sqrt(1.0 + up.y - right.x - back.z) * 2.0;
        return { (back.x - right.z) / s, (up.x + right.y) / s, 0.25 * s, (back.y + up.z) / s };
    }
    const double s = std::sqrt(1.0 + back.z - right.x - up.y) * 2.0;
    return { (right.y - up.x) / s, (back.x + right.z) / s, (back.y + up.z) / s, 0.25 * s };
}
