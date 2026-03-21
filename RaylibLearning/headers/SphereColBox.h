#pragma once

#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <chrono>
#include <vector>
#include "NoiseMap.h"

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
	bool resolveSphereTerrainCollision(NoiseMap &terrain) {

		Vector3 closestVertex = terrain.getClosestVertex(center);
		
		Vector3 dir = Vector3Normalize(Vector3Subtract(closestVertex, center));

		Ray ray;
		ray.position = center;
		ray.direction = dir;

		RayCollision col = GetRayCollisionMesh(ray, terrain.mesh, terrain.model.transform);

		if (col.hit && col.distance <= radius) {
			return true;
		}
		return false;
	}
};

