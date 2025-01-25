#pragma once

#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <chrono>

constexpr static float G_EARTH = 9.80665f;

class PhysicalObj
{
private:
	Vector3 Position;
	Mesh ObjMesh;
	Model Model;
	float ObjMass;
	Vector3 Velocity;

public:
	PhysicalObj(Vector3 pos, Mesh objmesh, float mass) {
		Position = pos;
		ObjMesh = objmesh;
		Model = LoadModelFromMesh(ObjMesh);
		ObjMass = mass;
		Velocity = { 0.f, 0.f, 0.f };
	}

	~PhysicalObj(){}

	void Gravity()
	{
		if (Position.y > 0)
		{
		AddVelocityObj(Vector3{ 0.f, -((G_EARTH) * ObjMass), 0.f });
		}
		else
		{
			Position.y = 0;
		}
	}

	void ChangeObjPos(Vector3 NewPos)
	{
		Position = NewPos;
	}

	void AddVelocityObj(Vector3 velocity)
	{
		Velocity += velocity;
	}

	void ApplyForceObj(Vector3 direction, float jouls)
	{
		Velocity += (direction*jouls);
	}

	void update(float deltaTime) {
		std::cout << "x: " << Velocity.x << " y: " << Velocity.y << " z: " << Velocity.z << std::endl;
		Gravity();
		Position = Position + (Velocity * deltaTime); // Update position based on velocity and deltaTime
	}

	void SetMaterialMapDiffuse(Texture2D texture) {
		Model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
	}

	void SetShader(Shader shader) {
		Model.materials[0].shader = shader;
	}

	void draw()
	{	
		DrawModel(Model, Position, 1.0f, WHITE);
	};
};

