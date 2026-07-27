#pragma once

#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <chrono>
#include <vector>
#include "Terrain.h"
#include "TerrainCollisionResponse.h"

class SphereColBox
{
public:
	Vector3 center;
	float radius;

	SphereColBox(Vector3 c,float r) {
		center = c;
		radius = r;
	}

	void ChangeCenterPos(Vector3 c) {
		center = c;
	}

	void ChangeRadius(float r) {
		radius = r;
	}
	bool ColisionStS(SphereColBox other) {
		float dist = Vector3Distance(center, other.center);
		if (dist <= (other.radius+radius))
		{
			return true;
		}
		else {
			return false;
		}
	}
	// maxWalkableAngleDeg: slopes shallower than this stop the sphere dead
	// (no downhill slide from gravity's tangential component). Slopes at or
	// past it are treated as too steep to stand on, so the old
	// friction/slide behavior still applies there. Default of 0 keeps every
	// slope in the "too steep" branch, i.e. today's always-slide behavior,
	// so existing callers (Marble) are unaffected unless they opt in.
	bool resolveSphereTerrainCollision(Terrain &terrain, Vector3& velocity, float maxWalkableAngleDeg = 0.0f) {

		// Sample the heightfield directly under the sphere rather than
		// raycasting toward the nearest mesh vertex: a ray aimed at the
		// nearest vertex comes in at a slant whenever the sphere isn't
		// sitting right above a vertex, so its hit distance overshoots the
		// true (vertical) penetration depth and the sphere falls through.
		// getHeightAt() is continuous, so this works the same everywhere on
		// the surface, vertex-aligned or not.
		float groundY = terrain.getHeightAt(center.x, center.z);
		float penetration = (groundY + radius) - center.y;

		if (penetration > 0.0f) {
			// Slope normal via central differences of the height field, so
			// this still deflects velocity correctly on sloped (NoiseMap)
			// terrain, not just flat ground.
			Vector3 normal = ComputeTerrainNormal(terrain, center.x, center.z);
			ApplyTerrainGroundResponse(center, velocity, normal, penetration, maxWalkableAngleDeg);
			return true;
		}
		return false;
	}
};

