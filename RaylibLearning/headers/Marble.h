#pragma once
#include "PhysicalObj.h"
#include "SphereColBox.h"


class Marble : public PhysicalObj
{
public:
	SphereColBox *colBox;
	float radius;
	bool isBouncy;

	Terrain* terrain = nullptr;  // store reference to terrain

	void setTerrain(Terrain* t) { terrain = t; }

	Marble(Vector3 pos, Mesh objmesh, float mass, float r, Color c, bool b)
		:PhysicalObj(pos, objmesh, mass) {
		colBox = new SphereColBox(pos, r); 
		radius = r;
		color = c;
		isBouncy = b;
	}

	~Marble() {
		delete colBox;  
	}

	void update(float deltaTime) override {
		inCollisionWithterrain = false;

		colBox->center = Position;

		if (terrain != nullptr && colBox->resolveSphereTerrainCollision(*terrain, Velocity)) {
			color = RED;
			inCollisionWithterrain = true;
			Position = colBox->center; 

			if (Velocity.y < 0) {
				Velocity.y *= -0.4f;  // bounce — 0.4 = 40% energy kept
				// 0.0 = no bounce, dead stop
				// 1.0 = perfect bounce, no energy loss
			}
		}

		PhysicalObj::update(deltaTime);
	}

	void debugPrint() {
		printf("pos:     %.2f %.2f %.2f\n", Position.x, Position.y, Position.z);
		printf("vel:     %.2f %.2f %.2f\n", Velocity.x, Velocity.y, Velocity.z);
		printf("falling: %s\n", inCollisionWithterrain ? "true" : "false");
		printf("\033[3A");  // go back up 3 lines — must match number of printfs
	}
};

