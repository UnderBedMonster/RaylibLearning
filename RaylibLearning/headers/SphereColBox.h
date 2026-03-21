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
	bool resolveSphereTerrainCollision(NoiseMap &terrain, Vector3& velocity) {

		Vector3 closestVertex = terrain.getClosestVertex(center);
		Vector3 dir = Vector3Normalize(Vector3Subtract(closestVertex, center));

		Ray ray;
		ray.position = center;
		ray.direction = dir;

		RayCollision col = GetRayCollisionMesh(ray, terrain.mesh, terrain.model.transform);

		if (col.hit && col.distance <= radius) {

			
			float sinkDepth = radius - col.distance;
			center = Vector3Add(center, Vector3Scale(col.normal, sinkDepth));
			//remove velocity going INTO surface
			float velDot = Vector3DotProduct(velocity, col.normal);
			if (velDot < 0) {
				velocity = Vector3Subtract(velocity, Vector3Scale(col.normal, velDot));


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

			


			return true;
		}
		return false;
	}
};

