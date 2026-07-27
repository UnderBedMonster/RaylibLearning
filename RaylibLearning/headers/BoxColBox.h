#pragma once

#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <chrono>
#include <vector>
#include "Terrain.h"
#include "TerrainCollisionResponse.h"

// Axis-aligned box collider (center + half-extents), with the same
// heightfield ground-collision approach as SphereColBox: sample the terrain
// under the box's footprint rather than raycasting the mesh.
class BoxColBox
{
public:
	Vector3 center;
	Vector3 halfExtents; // (halfWidth, halfHeight, halfDepth)

	BoxColBox(Vector3 c, Vector3 he) {
		center = c;
		halfExtents = he;
	}

	void ChangeCenterPos(Vector3 c) {
		center = c;
	}

	void ChangeHalfExtents(Vector3 he) {
		halfExtents = he;
	}

	bool ColisionBtB(BoxColBox other) {
		return (fabsf(center.x - other.center.x) <= (halfExtents.x + other.halfExtents.x)) &&
		       (fabsf(center.y - other.center.y) <= (halfExtents.y + other.halfExtents.y)) &&
		       (fabsf(center.z - other.center.z) <= (halfExtents.z + other.halfExtents.z));
	}

	// maxWalkableAngleDeg: see SphereColBox::resolveSphereTerrainCollision.
	bool resolveBoxTerrainCollision(Terrain &terrain, Vector3& velocity, float maxWalkableAngleDeg = 0.0f) {

		// Conservative ground height for the box's footprint: the highest of
		// its four bottom corners, so a corner can't clip into a slope while
		// the box's center still reads as clear.
		float x0 = center.x - halfExtents.x, x1 = center.x + halfExtents.x;
		float z0 = center.z - halfExtents.z, z1 = center.z + halfExtents.z;

		float groundY = terrain.getHeightAt(x0, z0);
		groundY = fmaxf(groundY, terrain.getHeightAt(x1, z0));
		groundY = fmaxf(groundY, terrain.getHeightAt(x0, z1));
		groundY = fmaxf(groundY, terrain.getHeightAt(x1, z1));

		float penetration = (groundY + halfExtents.y) - center.y;

		if (penetration > 0.0f) {
			Vector3 normal = ComputeTerrainNormal(terrain, center.x, center.z);
			ApplyTerrainGroundResponse(center, velocity, normal, penetration, maxWalkableAngleDeg);
			return true;
		}
		return false;
	}
};
