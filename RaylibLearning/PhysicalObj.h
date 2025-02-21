#pragma once

#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <chrono>
#include <vector>

constexpr static float G_EARTH = 9.80665f;

class PhysicalObj
{
public:

	Vector3 Position;
	Mesh ObjMesh;
	Model Model;
	float ObjMass;
	Vector3 Velocity;


	PhysicalObj(Vector3 pos, Mesh objmesh, float mass) {
		Position = pos;
		ObjMesh = objmesh;
		Model = LoadModelFromMesh(ObjMesh);
		ObjMass = mass;
		Velocity = { 0.f, 0.f, 0.f };
	}

	~PhysicalObj(){}

	void rotateQ(Quaternion q) {
		for (size_t i = 0; i < ObjMesh.vertexCount; i++)
		{

		}
	}

	std::vector<Vector2> flattenX()
	{
		std::vector<Vector2> FlattedToX = std::vector<Vector2>( ObjMesh.vertexCount );

		for (size_t i = 0; i < ObjMesh.vertexCount; i += 3)
		{
			FlattedToX[i] = Vector2{ ObjMesh.vertices[i]  , ObjMesh.vertices[i + 1] };
		}

		return FlattedToX;

	}

	void Gravity()
	{
		if (Position.y > 0.f)
		{
		AddVelocityObj(Vector3{ 0.f, -((G_EARTH) * ObjMass), 0.f });
		}
		else
		{
			Position.y = 0.f;
		}
	}

	bool BasicColision(){
		for (size_t i = 0; i < ObjMesh.vertexCount; i++)
		{
		    std::cout << "Vert " << i << " ";
			for (size_t j = 0; j < 3; j++)
			{
			    std::cout << ObjMesh.vertices[j] << " ; ";
			}
	      	std::cout << std::endl;
		}
		return true;
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
		//std::cout << "x: " << Velocity.x << " y: " << Velocity.y << " z: " << Velocity.z << std::endl;
		Position = Position + (Velocity * deltaTime); // Update position based on velocity and deltaTime
		Gravity();
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

