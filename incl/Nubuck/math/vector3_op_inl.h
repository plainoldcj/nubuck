#pragma once

#include "math.h"
#include "vector3.h"

namespace M {

	M_INLINE bool operator==(const Vector3& lhp, const Vector3& rhp) {
		return lhp.x == rhp.x && lhp.y == rhp.y && lhp.z == rhp.z;
	}

	M_INLINE Vector3 operator+(const Vector3& lhp, const Vector3& rhp) {
		return Vector3(lhp.x + rhp.x, lhp.y + rhp.y, lhp.z + rhp.z);
	}

	M_INLINE Vector3 operator-(const Vector3& lhp, const Vector3& rhp) {
		return Vector3(lhp.x - rhp.x, lhp.y - rhp.y, lhp.z - rhp.z);
	}

	M_INLINE Vector3 operator*(float scalar, const Vector3& vector) {
		return Vector3(scalar * vector.x, scalar * vector.y, scalar * vector.z);
	}

	M_INLINE Vector3 operator*(const Vector3& vector, float scalar) {
		return Vector3(scalar * vector.x, scalar * vector.y, scalar * vector.z);
	}

	M_INLINE Vector3 operator/(const Vector3& vector, float scalar) {
		return Vector3(vector.x / scalar, vector.y / scalar, vector.z / scalar);
	}

	M_INLINE Vector3 Negate(const Vector3& vec) {
		return Vector3(-vec.x, -vec.y, -vec.z);
	}

	M_INLINE float Dot(const Vector3& lhp, const Vector3& rhp) {
		return lhp.x * rhp.x + lhp.y * rhp.y + lhp.z * rhp.z;
	}

	M_INLINE Vector3 Cross(const Vector3& lhp, const Vector3& rhp) {
		return Vector3(lhp.y * rhp.z - lhp.z * rhp.y,
					   lhp.z * rhp.x - lhp.x * rhp.z,
					   lhp.x * rhp.y - lhp.y * rhp.x);
	}

    M_INLINE Vector3 Lerp(const Vector3& v0, const Vector3& v1, float t) {
        return (1.0f - t) * v0 + t * v1;
    }

} // namespace M
