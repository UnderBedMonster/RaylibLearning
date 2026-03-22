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
	bool resolveSphereTerrainCollision(Terrain &terrain, Vector3& velocity) {

		Vector3 closestVertex = terrain.getClosestVertex(center);
		Vector3 dir = Vector3Normalize(Vector3Subtract(closestVertex, center));

		Ray ray;
		ray.position = center;
		ray.direction = dir;

		RayCollision col = GetRayCollisionMesh(ray, terrain.mesh, terrain.model.transform);
		
		if (col.hit && col.distance <= radius) {
			// push out
			float sinkDepth = radius - col.distance;
			center = Vector3Add(center, Vector3Scale(col.normal, sinkDepth));

			// reflect velocity along normal with energy loss
			float velDot = Vector3DotProduct(velocity, col.normal);
			if (velDot < 0) {
				float restitution = 0.4f;  // bounciness 0-1
				velocity = Vector3Subtract(velocity,
					Vector3Scale(col.normal, velDot * (1.0f + restitution)));
			}

			// rolling friction — gradually kills horizontal movement
			float friction = 0.95f;  // per frame — tune this
			velocity.x *= friction;
			velocity.z *= friction;

			// stop completely if moving very slowly
			if (Vector3Length(velocity) < 0.05f) {
				velocity = { 0, 0, 0 };
			}

			return true;
		}
		return false;
	}
};

