#pragma once
#include <raylib.h>
#include <raymath.h>
#include <cmath>
#include "PhysicalObj.h"
#include "BoxColBox.h"
#include "Terrain.h"

// First-person-controlled player: a PhysicalObj (so it gets gravity and
// terrain collision the same way Marble does) plus WASD + mouse-look input
// and an eye point the camera is synced to every frame (see main.cpp).
class Player : public PhysicalObj
{
public:
	BoxColBox* colBox;
	Vector3 halfExtents; // (halfWidth, halfHeight, halfDepth) of the collision box
	float eyeHeight;

	float yaw = 0.0f;   // radians, rotation around world up (Y)
	float pitch = 0.0f; // radians, look up/down

	float moveSpeed = 10.0f;
	float mouseSensitivity = 0.003f;

	Terrain* terrain = nullptr;
	void setTerrain(Terrain* t) { terrain = t; }

	Player(Vector3 pos, Mesh objmesh, float mass, Vector3 halfExtentsIn, float eyeHeightIn, Color c)
		: PhysicalObj(pos, objmesh, mass) {
		colBox = new BoxColBox(pos, halfExtentsIn);
		halfExtents = halfExtentsIn;
		eyeHeight = eyeHeightIn;
		color = c;
	}

	~Player() {
		delete colBox;
	}

	Vector3 GetForward() const {
		return Vector3{
			cosf(pitch) * sinf(yaw),
			sinf(pitch),
			cosf(pitch) * cosf(yaw)
		};
	}

	Vector3 GetEyePosition() const {
		return Vector3{ Position.x, Position.y + eyeHeight, Position.z };
	}

	// Mouse-look + WASD ground movement. Call once per frame, before update().
	void HandleInput(float deltaTime) {
		Vector2 mouseDelta = GetMouseDelta();
		yaw   -= mouseDelta.x * mouseSensitivity;
		pitch -= mouseDelta.y * mouseSensitivity;

		constexpr float pitchLimit = 89.0f * DEG2RAD;
		if (pitch > pitchLimit)  pitch = pitchLimit;
		if (pitch < -pitchLimit) pitch = -pitchLimit;

		// Flattened (pitch-independent), so looking up/down doesn't change ground speed.
		Vector3 forward = { sinf(yaw), 0.0f, cosf(yaw) };
		Vector3 right   = { cosf(yaw), 0.0f, -sinf(yaw) };

		Vector3 move = { 0.0f, 0.0f, 0.0f };
		if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
		if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forward);
		if (IsKeyDown(KEY_D)) move = Vector3Subtract(move, right);
		if (IsKeyDown(KEY_A)) move = Vector3Add(move, right);
		if (IsKeyDown(KEY_LEFT_SHIFT)) moveSpeed = 20.0f;
		else moveSpeed = 10.0f;

		if (Vector3LengthSqr(move) > 0.0f) {
			move = Vector3Scale(Vector3Normalize(move), moveSpeed * deltaTime);
			Position = Vector3Add(Position, move);
		}

		rotation = QuaternionR(yaw, Vector3{ 0.0f, 1.0f, 0.0f });
	}

	// Slopes shallower than this are walkable (no downhill slide); at or
	// past it, the player slides down instead of sticking to the surface.
	float maxWalkableAngleDeg = 60.0f;

	void update(float deltaTime) override {
		inCollisionWithterrain = false;
		colBox->center = Position;

		if (terrain != nullptr && colBox->resolveBoxTerrainCollision(*terrain, Velocity, maxWalkableAngleDeg)) {
			inCollisionWithterrain = true;
			Velocity.y = 0.0f;
			Position = colBox->center;
		}

		PhysicalObj::update(deltaTime);
	}
};
