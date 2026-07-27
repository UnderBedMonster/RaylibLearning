#pragma once

#include <raylib.h>
#include <raymath.h>
#include "Terrain.h"

// Ground-response step shared by every terrain collider (SphereColBox,
// BoxColBox, ...): pushes the collider out of the surface along `normal` by
// `penetration`, then either kills the remaining tangential velocity
// (slopeAngleDeg < maxWalkableAngleDeg — walkable ground, no downhill slide)
// or applies light friction while sliding down slopes steeper than that.
inline void ApplyTerrainGroundResponse(Vector3& center, Vector3& velocity, Vector3 normal,
	float penetration, float maxWalkableAngleDeg) {

	center = Vector3Add(center, Vector3Scale(normal, penetration));

	//remove velocity going INTO surface
	float velDot = Vector3DotProduct(velocity, normal);
	if (velDot < 0) {
		velocity = Vector3Subtract(velocity, Vector3Scale(normal, velDot));

		float slopeAngleDeg = acosf(Clamp(normal.y, -1.0f, 1.0f)) * RAD2DEG;

		if (slopeAngleDeg < maxWalkableAngleDeg) {
			// Walkable ground: kill the remaining tangential velocity too, so
			// gravity doesn't slide the collider downhill.
			velocity = { 0.0f, 0.0f, 0.0f };
		}
		else {
			// Too steep to stand on: let it slide down the slope.
			float speed = Vector3Length(velocity);
			if (speed > 0.1f) {
				float frictionForce = 0.0001f;  // tune this — lower = icier
				Vector3 frictionDir = Vector3Negate(Vector3Normalize(velocity));
				velocity = Vector3Add(velocity,
					Vector3Scale(frictionDir, frictionForce * speed));
			}
			else {
				velocity = { 0, 0, 0 };  // stop fully if nearly still
			}
		}
	}
}

// Slope normal via central differences of the terrain height field at (x, z).
inline Vector3 ComputeTerrainNormal(Terrain& terrain, float x, float z, float eps = 0.5f) {
	float hL = terrain.getHeightAt(x - eps, z);
	float hR = terrain.getHeightAt(x + eps, z);
	float hD = terrain.getHeightAt(x, z - eps);
	float hU = terrain.getHeightAt(x, z + eps);
	float dhdx = (hR - hL) / (2.0f * eps);
	float dhdz = (hU - hD) / (2.0f * eps);
	return Vector3Normalize(Vector3{ -dhdx, 1.0f, -dhdz });
}
